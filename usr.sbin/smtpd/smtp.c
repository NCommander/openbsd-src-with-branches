/*	$OpenBSD: smtp.c,v 1.71 2010/05/19 20:57:10 gilles Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <event.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "smtpd.h"

void		smtp_imsg(struct smtpd *, struct imsgev *, struct imsg *);
__dead void	smtp_shutdown(void);
void		smtp_sig_handler(int, short, void *);
void		smtp_setup_events(struct smtpd *);
void		smtp_disable_events(struct smtpd *);
void		smtp_pause(struct smtpd *);
int		smtp_enqueue(struct smtpd *, uid_t *);
void		smtp_accept(int, short, void *);
struct session *smtp_new(struct listener *);
struct session *session_lookup(struct smtpd *, u_int64_t);

void
smtp_imsg(struct smtpd *env, struct imsgev *iev, struct imsg *imsg)
{
	struct session		 skey;
	struct submit_status	*ss;
	struct listener		*l;
	struct session		*s;
	struct auth		*auth;
	struct ssl		*ssl;
	struct dns		*dns;

	if (iev->proc == PROC_LKA) {
		switch (imsg->hdr.type) {
		case IMSG_DNS_PTR:
			dns = imsg->data;
			s = session_lookup(env, dns->id);
			if (s == NULL)
				fatalx("smtp: impossible quit");
			strlcpy(s->s_hostname,
			    dns->error ? "<unknown>" : dns->host,
			    sizeof s->s_hostname);
			strlcpy(s->s_msg.session_hostname, s->s_hostname,
			    sizeof s->s_msg.session_hostname);
			session_init(s->s_l, s);
			return;
		}
	}

	if (iev->proc == PROC_MFA) {
		switch (imsg->hdr.type) {
		case IMSG_MFA_RCPT:
		case IMSG_MFA_MAIL:
			log_debug("smtp: got imsg_mfa_mail/rcpt");
			ss = imsg->data;
			s = session_lookup(env, ss->id);
			if (s == NULL)
				return;
			session_pickup(s, ss);
			return;
		}
	}

	if (iev->proc == PROC_QUEUE) {
		ss = imsg->data;

		switch (imsg->hdr.type) {
		case IMSG_QUEUE_CREATE_MESSAGE:
			log_debug("smtp: imsg_queue_create_message returned");
			s = session_lookup(env, ss->id);
			if (s == NULL)
				return;
			strlcpy(s->s_msg.message_id, ss->u.msgid,
			    sizeof s->s_msg.message_id);
			session_pickup(s, ss);
			return;

		case IMSG_QUEUE_MESSAGE_FILE:
			log_debug("smtp: imsg_queue_message_file returned");
			s = session_lookup(env, ss->id);
			if (s == NULL) {
				close(imsg->fd);
				return;
			}
			s->datafp = fdopen(imsg->fd, "w");
			if (s->datafp == NULL) {
				/* queue may have experienced tempfail. */
				if (ss->code != 421)
					fatalx("smtp: fdopen");
				close(imsg->fd);
			}
			session_pickup(s, ss);
			return;

		case IMSG_QUEUE_TEMPFAIL:
			log_debug("smtp: got imsg_queue_tempfail");
			skey.s_id = ss->id;
			s = SPLAY_FIND(sessiontree, &env->sc_sessions, &skey);
			if (s == NULL)
				fatalx("smtp: session is gone");
			if (s->s_flags & F_WRITEONLY)
				/* session is write-only, must not destroy it. */
				s->s_msg.status |= S_MESSAGE_TEMPFAILURE;
			else
				fatalx("smtp: corrupt session");
			return;

		case IMSG_QUEUE_COMMIT_ENVELOPES:
			log_debug("smtp: got imsg_queue_commit_envelopes");
			s = session_lookup(env, ss->id);
			if (s == NULL)
				return;
			session_pickup(s, ss);
			return;

		case IMSG_QUEUE_COMMIT_MESSAGE:
			log_debug("smtp: got imsg_queue_commit_message");
			s = session_lookup(env, ss->id);
			if (s == NULL)
				return;
			session_pickup(s, ss);
			return;

		case IMSG_SMTP_ENQUEUE:
			imsg_compose_event(iev, IMSG_SMTP_ENQUEUE, 0, 0,
			    smtp_enqueue(env, NULL), imsg->data,
			    sizeof(struct message));
			return;
		}
	}

	if (iev->proc == PROC_PARENT) {
		switch (imsg->hdr.type) {
		case IMSG_CONF_RELOAD:
			/*
			 * Reloading may invalidate various pointers our
			 * sessions rely upon, we better tell clients we
			 * want them to retry.
			 */
			SPLAY_FOREACH(s, sessiontree, &env->sc_sessions) {
				s->s_l = NULL;
				s->s_msg.status |= S_MESSAGE_TEMPFAILURE;
			}
			if (env->sc_listeners)
				smtp_disable_events(env);
			imsg_compose_event(iev, IMSG_PARENT_SEND_CONFIG, 0, 0, -1,
			    NULL, 0);
			return;

		case IMSG_CONF_START:
			if (env->sc_flags & SMTPD_CONFIGURING)
				return;
			env->sc_flags |= SMTPD_CONFIGURING;
			env->sc_listeners = calloc(1, sizeof *env->sc_listeners);
			env->sc_ssl = calloc(1, sizeof *env->sc_ssl);
			if (env->sc_listeners == NULL || env->sc_ssl == NULL)
				fatal(NULL);
			TAILQ_INIT(env->sc_listeners);
			return;

		case IMSG_CONF_SSL:
			if (!(env->sc_flags & SMTPD_CONFIGURING))
				return;
			ssl = calloc(1, sizeof *ssl);
			if (ssl == NULL)
				fatal(NULL);
			*ssl = *(struct ssl *)imsg->data;
			ssl->ssl_cert = strdup((char *)imsg->data +
			    sizeof *ssl);
			if (ssl->ssl_cert == NULL)
				fatal(NULL);
			ssl->ssl_key = strdup((char *)imsg->data + sizeof *ssl +
			    ssl->ssl_cert_len);
			if (ssl->ssl_key == NULL)
				fatal(NULL);
			SPLAY_INSERT(ssltree, env->sc_ssl, ssl);
			return;

		case IMSG_CONF_LISTENER:
			if (!(env->sc_flags & SMTPD_CONFIGURING))
				return;
			l = calloc(1, sizeof *l);
			if (l == NULL)
				fatal(NULL);
			*l = *(struct listener *)imsg->data;
			l->fd = imsg->fd;
			if (l->fd < 0)
				fatalx("smtp: listener pass failed");
			if (l->flags & F_SSL) {
				struct ssl key;

				strlcpy(key.ssl_name, l->ssl_cert_name,
				    sizeof key.ssl_name);
				l->ssl = SPLAY_FIND(ssltree, env->sc_ssl, &key);
				if (l->ssl == NULL)
					fatalx("smtp: ssltree out of sync");
			}
			TAILQ_INSERT_TAIL(env->sc_listeners, l, entry);
			return;

		case IMSG_CONF_END:
			if (!(env->sc_flags & SMTPD_CONFIGURING))
				return;
			smtp_setup_events(env);
			env->sc_flags &= ~SMTPD_CONFIGURING;
			return;

		case IMSG_PARENT_AUTHENTICATE:
			auth = imsg->data;
			s = session_lookup(env, auth->id);
			if (s == NULL)
				return;
			if (auth->success) {
				s->s_flags |= F_AUTHENTICATED;
				s->s_msg.flags |= F_MESSAGE_AUTHENTICATED;
			} else {
				s->s_flags &= ~F_AUTHENTICATED;
				s->s_msg.flags &= ~F_MESSAGE_AUTHENTICATED;
			}
			session_pickup(s, NULL);
			return;

		case IMSG_CTL_VERBOSE:
			log_verbose(*(int *)imsg->data);
			return;
		}
	}

	if (iev->proc == PROC_CONTROL) {
		switch (imsg->hdr.type) {
		case IMSG_SMTP_ENQUEUE:
			imsg_compose_event(iev, IMSG_SMTP_ENQUEUE,
			    imsg->hdr.peerid, 0, smtp_enqueue(env, imsg->data),
			    NULL, 0);
			return;

		case IMSG_SMTP_PAUSE:
			smtp_pause(env);
			return;

		case IMSG_SMTP_RESUME:
			smtp_resume(env);
			return;
		}
	}

	fatalx("smtp_imsg: unexpected imsg");
}

void
smtp_sig_handler(int sig, short event, void *p)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		smtp_shutdown();
		break;
	default:
		fatalx("smtp_sig_handler: unexpected signal");
	}
}

void
smtp_shutdown(void)
{
	log_info("smtp server exiting");
	_exit(0);
}

pid_t
smtp(struct smtpd *env)
{
	pid_t		 pid;
	struct passwd	*pw;

	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	struct peer peers[] = {
		{ PROC_PARENT,	imsg_dispatch },
		{ PROC_MFA,	imsg_dispatch },
		{ PROC_QUEUE,	imsg_dispatch },
		{ PROC_LKA,	imsg_dispatch },
		{ PROC_CONTROL,	imsg_dispatch }
	};

	switch (pid = fork()) {
	case -1:
		fatal("smtp: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	ssl_init();
	purge_config(env, PURGE_EVERYTHING);

	pw = env->sc_pw;

	if (chroot(pw->pw_dir) == -1)
		fatal("smtp: chroot");
	if (chdir("/") == -1)
		fatal("smtp: chdir(\"/\")");

	smtpd_process = PROC_SMTP;
	setproctitle("%s", env->sc_title[smtpd_process]);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("smtp: cannot drop privileges");

	imsg_callback = smtp_imsg;
	event_init();

	signal_set(&ev_sigint, SIGINT, smtp_sig_handler, env);
	signal_set(&ev_sigterm, SIGTERM, smtp_sig_handler, env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	/* Initial limit for use by IMSG_SMTP_ENQUEUE, will be tuned later once
	 * the listening sockets arrive. */
	env->sc_maxconn = availdesc() / 2;

	config_pipes(env, peers, nitems(peers));
	config_peers(env, peers, nitems(peers));

	event_dispatch();
	smtp_shutdown();

	return (0);
}

void
smtp_setup_events(struct smtpd *env)
{
	struct listener *l;
	int avail = availdesc();

	TAILQ_FOREACH(l, env->sc_listeners, entry) {
		log_debug("smtp_setup_events: listen on %s port %d flags 0x%01x"
		    " cert \"%s\"", ss_to_text(&l->ss), ntohs(l->port),
		    l->flags, l->ssl_cert_name);

		session_socket_blockmode(l->fd, BM_NONBLOCK);
		if (listen(l->fd, SMTPD_BACKLOG) == -1)
			fatal("listen");
		l->env = env;
		event_set(&l->ev, l->fd, EV_READ|EV_PERSIST, smtp_accept, l);
		event_add(&l->ev, NULL);
		ssl_setup(env, l);
		avail--;
	}

	/* guarantee 2 fds to each accepted client */
	if ((env->sc_maxconn = avail / 2) < 1)
		fatalx("smtp_setup_events: fd starvation");

	log_debug("smtp: will accept at most %d clients", env->sc_maxconn);
}

void
smtp_disable_events(struct smtpd *env)
{
	struct listener	*l;

	log_debug("smtp_disable_events: closing listening sockets");
	while ((l = TAILQ_FIRST(env->sc_listeners)) != NULL) {
		TAILQ_REMOVE(env->sc_listeners, l, entry);
		event_del(&l->ev);
		close(l->fd);
		free(l);
	}
	free(env->sc_listeners);
	env->sc_listeners = NULL;
	env->sc_maxconn = 0;
}

void
smtp_pause(struct smtpd *env)
{
	struct listener *l;

	log_debug("smtp_pause: pausing listening sockets");
	env->sc_opts |= SMTPD_SMTP_PAUSED;

	TAILQ_FOREACH(l, env->sc_listeners, entry)
		event_del(&l->ev);
}

void
smtp_resume(struct smtpd *env)
{
	struct listener *l;

	log_debug("smtp_resume: resuming listening sockets");
	env->sc_opts &= ~SMTPD_SMTP_PAUSED;

	TAILQ_FOREACH(l, env->sc_listeners, entry)
		event_add(&l->ev, NULL);
}

int
smtp_enqueue(struct smtpd *env, uid_t *euid)
{
	static struct listener		 local, *l;
	static struct sockaddr_storage	 sa;
	struct session			*s;
	int				 fd[2];

	if (l == NULL) {
		struct addrinfo hints, *res;

		l = &local;
		strlcpy(l->tag, "local", sizeof(l->tag));

		bzero(&hints, sizeof(hints));
		hints.ai_family = PF_UNSPEC;
		hints.ai_flags = AI_NUMERICHOST;

		if (getaddrinfo("::1", NULL, &hints, &res))
			fatal("getaddrinfo");
		memcpy(&sa, res->ai_addr, res->ai_addrlen);
		freeaddrinfo(res);
	}
	l->env = env;

	/*
	 * Some enqueue requests buffered in IMSG may still arrive even after
	 * call to smtp_pause() because enqueue listener is not a real socket
	 * and thus cannot be paused properly.
	 */
	if (env->sc_opts & SMTPD_SMTP_PAUSED)
		return (-1);

	if ((s = smtp_new(l)) == NULL)
		return (-1);

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, fd))
		fatal("socketpair");

	s->s_fd = fd[0];
	s->s_ss = sa;
	s->s_msg.flags |= F_MESSAGE_ENQUEUED;

	if (euid)
		bsnprintf(s->s_hostname, sizeof(s->s_hostname), "%d@localhost",
		    *euid);
	else {
		strlcpy(s->s_hostname, "localhost", sizeof(s->s_hostname));
		s->s_msg.flags |= F_MESSAGE_BOUNCE;
	}

	strlcpy(s->s_msg.session_hostname, s->s_hostname,
	    sizeof(s->s_msg.session_hostname));

	session_init(l, s);

	return (fd[1]);
}

void
smtp_accept(int fd, short event, void *p)
{
	struct listener		*l = p;
	struct session		*s;
	socklen_t		 len;

	if ((s = smtp_new(l)) == NULL)
		return;

	len = sizeof(s->s_ss);
	if ((s->s_fd = accept(fd, (struct sockaddr *)&s->s_ss, &len)) == -1) {
		if (errno == EINTR || errno == ECONNABORTED)
			return;
		fatal("smtp_accept");
	}

	s->s_flags |= F_WRITEONLY;
	dns_query_ptr(l->env, &s->s_ss, s->s_id);
}


struct session *
smtp_new(struct listener *l)
{
	struct smtpd	*env = l->env;
	struct session	*s;

	log_debug("smtp_new: incoming client on listener: %p", l);

	if (env->sc_opts & SMTPD_SMTP_PAUSED)
		fatalx("smtp_new: unexpected client");

	if (env->stats->smtp.sessions_active >= env->sc_maxconn) {
		log_warnx("client limit hit, disabling incoming connections");
		smtp_pause(env);
		return (NULL);
	}
	
	if ((s = calloc(1, sizeof(*s))) == NULL)
		fatal(NULL);
	s->s_id = generate_uid();
	s->s_env = env;
	s->s_l = l;
	strlcpy(s->s_msg.tag, l->tag, sizeof(s->s_msg.tag));
	SPLAY_INSERT(sessiontree, &env->sc_sessions, s);

	env->stats->smtp.sessions++;
	env->stats->smtp.sessions_active++;

	return (s);
}

/*
 * Helper function for handling IMSG replies.
 */
struct session *
session_lookup(struct smtpd *env, u_int64_t id)
{
	struct session	 key;
	struct session	*s;

	key.s_id = id;
	s = SPLAY_FIND(sessiontree, &env->sc_sessions, &key);
	if (s == NULL)
		fatalx("session_lookup: session is gone");

	if (!(s->s_flags & F_WRITEONLY))
		fatalx("session_lookup: corrupt session");
	s->s_flags &= ~F_WRITEONLY;

	if (s->s_flags & F_QUIT) {
		session_destroy(s);
		s = NULL;
	}

	return (s);
}
