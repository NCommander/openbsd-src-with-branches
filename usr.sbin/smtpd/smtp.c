/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
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

#include <ctype.h>
#include <event.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"

__dead void	smtp_shutdown(void);
void		smtp_sig_handler(int, short, void *);
void		smtp_dispatch_parent(int, short, void *);
void		smtp_dispatch_mfa(int, short, void *);
void		smtp_dispatch_lka(int, short, void *);
void		smtp_dispatch_queue(int, short, void *);
void		smtp_setup_events(struct smtpd *);
void		smtp_disable_events(struct smtpd *);
void		smtp_accept(int, short, void *);
void		session_timeout(int, short, void *);

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
smtp_dispatch_parent(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_PARENT];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("parent_dispatch_smtp: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_CONF_START:
			if (env->sc_flags & SMTPD_CONFIGURING)
				break;
			env->sc_flags |= SMTPD_CONFIGURING;
			smtp_disable_events(env);
			break;
		case IMSG_CONF_SSL: {
			struct ssl	*s;
			struct ssl	*x_ssl;

			if (!(env->sc_flags & SMTPD_CONFIGURING))
				break;

			if ((s = calloc(1, sizeof(*s))) == NULL)
				fatal(NULL);
			x_ssl = imsg.data;
			(void)strlcpy(s->ssl_name, x_ssl->ssl_name,
			    sizeof(s->ssl_name));
			s->ssl_cert_len = x_ssl->ssl_cert_len;
			if ((s->ssl_cert = malloc(s->ssl_cert_len + 1)) == NULL)
				fatal(NULL);
			(void)strlcpy(s->ssl_cert,
			    (char *)imsg.data + sizeof(*s),
			    s->ssl_cert_len);

			s->ssl_key_len = x_ssl->ssl_key_len;
			if ((s->ssl_key = malloc(s->ssl_key_len + 1)) == NULL)
				fatal(NULL);
			(void)strlcpy(s->ssl_key,
			    (char *)imsg.data + (sizeof(*s) + s->ssl_cert_len),
			    s->ssl_key_len);

			SPLAY_INSERT(ssltree, &env->sc_ssl, s);
			break;
		}
		case IMSG_CONF_LISTENER: {
			struct listener	*l;
			struct ssl	 key;

			if (!(env->sc_flags & SMTPD_CONFIGURING))
				break;

			if ((l = calloc(1, sizeof(*l))) == NULL)
				fatal(NULL);
			memcpy(l, imsg.data, sizeof(*l));
			if ((l->fd = imsg_get_fd(ibuf, &imsg)) == -1)
				fatal("cannot get fd");

			log_debug("smtp_dispatch_parent: "
			    "got fd %d for listener: %p", l->fd, l);

			(void)strlcpy(key.ssl_name, l->ssl_cert_name,
			    sizeof(key.ssl_name));

			if (l->flags & F_SSL)
				if ((l->ssl = SPLAY_FIND(ssltree,
				    &env->sc_ssl, &key)) == NULL)
					fatal("parent and smtp desynchronized");

			TAILQ_INSERT_TAIL(&env->sc_listeners, l, entry);
			break;
		}
		case IMSG_CONF_END:
			if (!(env->sc_flags & SMTPD_CONFIGURING))
				break;
			smtp_setup_events(env);
			env->sc_flags &= ~SMTPD_CONFIGURING;
			break;
		case IMSG_PARENT_AUTHENTICATE: {
			struct session		*s;
			struct session		 key;
			struct session_auth_reply *reply;

			log_debug("smtp_dispatch_parent: parent handled authentication");
			reply = imsg.data;
			key.s_id = reply->session_id;
			key.s_msg.id = reply->session_id;

			s = SPLAY_FIND(sessiontree, &env->sc_sessions, &key);
			if (s == NULL) {
				/* Session was removed while we were waiting for the message */
				break;
			}

			if (reply->value)
				s->s_flags |= F_AUTHENTICATED;

			session_pickup(s, NULL);

			break;
		}
		default:
			log_debug("parent_dispatch_smtp: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
smtp_dispatch_mfa(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_MFA];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("smtp_dispatch_mfa: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_MFA_RCPT_SUBMIT:
		case IMSG_MFA_RPATH_SUBMIT: {
			struct submit_status	*ss;
			struct session		*s;
			struct session		 key;

			log_debug("smtp_dispatch_mfa: mfa handled return path");
			ss = imsg.data;
			key.s_id = ss->id;
			key.s_msg.id = ss->id;

			s = SPLAY_FIND(sessiontree, &env->sc_sessions, &key);
			if (s == NULL) {
				/* Session was removed while we were waiting for the message */
				break;
			}

			session_pickup(s, ss);
			break;
		}
		default:
			log_debug("smtp_dispatch_mfa: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
smtp_dispatch_lka(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_LKA];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("smtp_dispatch_lka: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_SMTP_HOSTNAME_ANSWER: {
			struct session		 key;
			struct session		*s;
			struct session		*ss;

			s = imsg.data;
			key.s_id = s->s_id;

			ss = SPLAY_FIND(sessiontree, &env->sc_sessions, &key);
			if (ss == NULL) {
				/* Session was removed while we were waiting for the message */
				break;
			}

			strlcpy(ss->s_hostname, s->s_hostname, MAXHOSTNAMELEN);

			break;
		}
		default:
			log_debug("smtp_dispatch_lka: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
smtp_dispatch_queue(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_QUEUE];
	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&ibuf->ev);
			event_loopexit(NULL);
			return;
		}
		break;
	case EV_WRITE:
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
		imsg_event_add(ibuf);
		return;
	default:
		fatalx("unknown event");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("smtp_dispatch_queue: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_SMTP_MESSAGE_FILE: {
			struct submit_status	*ss;
			struct session		*s;
			struct session		 key;
			int			 fd;

			log_debug("smtp_dispatch_queue: queue handled message creation");
			ss = imsg.data;

			key.s_id = ss->id;

			s = SPLAY_FIND(sessiontree, &env->sc_sessions, &key);
			if (s == NULL) {
				/* Session was removed while we were waiting for the message */
				break;
			}

			(void)strlcpy(s->s_msg.message_id, ss->u.msgid,
			    sizeof(s->s_msg.message_id));

			fd = imsg_get_fd(ibuf, &imsg);
			if (fd != -1) {
				s->s_msg.datafp = fdopen(fd, "w");
				if (s->s_msg.datafp == NULL) {
					/* no need to handle error, it will be
					 * caught in session_pickup()
					 */
					close(fd);
				}
			}
			session_pickup(s, ss);

			break;
		}
		case IMSG_SMTP_SUBMIT_ACK: {
			struct submit_status	*ss;
			struct session		*s;
			struct session		 key;

			log_debug("smtp_dispatch_queue: queue acknowledged message submission");
			ss = imsg.data;
			key.s_id = ss->id;
			key.s_msg.id = ss->id;

			s = SPLAY_FIND(sessiontree, &env->sc_sessions, &key);
			if (s == NULL) {
				/* Session was removed while we were waiting for the message */
				break;
			}

			(void)strlcpy(s->s_msg.message_id, ss->u.msgid,
			    sizeof(s->s_msg.message_id));

			session_pickup(s, ss);

			break;
		}
		default:
			log_debug("smtp_dispatch_queue: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
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
		{ PROC_PARENT,	smtp_dispatch_parent },
		{ PROC_MFA,	smtp_dispatch_mfa },
		{ PROC_QUEUE,	smtp_dispatch_queue },
		{ PROC_LKA,	smtp_dispatch_lka }
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

#ifndef DEBUG
	if (chroot(pw->pw_dir) == -1)
		fatal("smtp: chroot");
	if (chdir("/") == -1)
		fatal("smtp: chdir(\"/\")");
#else
#warning disabling privilege revocation and chroot in DEBUG MODE
#endif

	setproctitle("smtp server");
	smtpd_process = PROC_SMTP;

#ifndef DEBUG
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("smtp: cannot drop privileges");
#endif

	event_init();

	signal_set(&ev_sigint, SIGINT, smtp_sig_handler, env);
	signal_set(&ev_sigterm, SIGTERM, smtp_sig_handler, env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	config_peers(env, peers, 4);

	smtp_setup_events(env);
	event_dispatch();
	smtp_shutdown();

	return (0);
}

void
smtp_setup_events(struct smtpd *env)
{
	struct listener *l;
	struct timeval	 tv;

	TAILQ_FOREACH(l, &env->sc_listeners, entry) {
		log_debug("smtp_setup_events: configuring listener: %p%s.",
		    l, (l->flags & F_SSL)?" (with ssl)":"");

		if (fcntl(l->fd, F_SETFL, O_NONBLOCK) == -1)
			fatal("fcntl");
		if (listen(l->fd, l->backlog) == -1)
			fatal("listen");
		l->env = env;
		event_set(&l->ev, l->fd, EV_READ, smtp_accept, l);
		event_add(&l->ev, NULL);
		ssl_setup(env, l);
	}

	evtimer_set(&env->sc_ev, session_timeout, env);
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	evtimer_add(&env->sc_ev, &tv);
}

void
smtp_disable_events(struct smtpd *env)
{
	struct listener	*l;

	log_debug("smtp_disable_events: closing listening sockets");
	while ((l = TAILQ_FIRST(&env->sc_listeners)) != NULL) {
		TAILQ_REMOVE(&env->sc_listeners, l, entry);
		event_del(&l->ev);
		close(l->fd);
		free(l);
	}
	TAILQ_INIT(&env->sc_listeners);
}

void
smtp_accept(int fd, short event, void *p)
{
	int			 s_fd;
	struct sockaddr_storage	 ss;
	struct listener		*l = p;
	struct session		*s;
	socklen_t		 len;

	log_debug("smtp_accept: incoming client on listener: %p", l);
	len = sizeof(struct sockaddr_storage);
	if ((s_fd = accept(l->fd, (struct sockaddr *)&ss, &len)) == -1) {
		event_add(&l->ev, NULL);
		return;
	}

	if ((s = calloc(1, sizeof(*s))) == NULL)
		fatal(NULL);
	len = sizeof(s->s_ss);

	s->s_fd = s_fd;
	s->s_tm = time(NULL);
	(void)memcpy(&s->s_ss, &ss, sizeof(s->s_ss));

	session_init(l, s);
	event_add(&l->ev, NULL);
}

void
smtp_listener_setup(struct smtpd *env, struct listener *l)
{
	int opt;

	if ((l->fd = socket(l->ss.ss_family, SOCK_STREAM, 0)) == -1)
		fatal("socket");

	opt = 1;
	setsockopt(l->fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	if (bind(l->fd, (struct sockaddr *)&l->ss, l->ss.ss_len) == -1)
		fatal("bind");
}
