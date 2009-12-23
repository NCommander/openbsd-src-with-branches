/*	$OpenBSD: control.c,v 1.43 2009/12/13 22:02:55 jacekm Exp $	*/

/*
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
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
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"

#define CONTROL_BACKLOG 5

/* control specific headers */
struct {
	struct event		 ev;
	int			 fd;
} control_state;

__dead void	 control_shutdown(void);
int		 control_init(void);
void		 control_listen(struct smtpd *);
void		 control_cleanup(void);
void		 control_accept(int, short, void *);
struct ctl_conn	*control_connbyfd(int);
void		 control_close(struct smtpd *, int);
void		 control_sig_handler(int, short, void *);
void		 control_dispatch_ext(int, short, void *);
void		 control_dispatch_lka(int, short, void *);
void		 control_dispatch_mfa(int, short, void *);
void		 control_dispatch_queue(int, short, void *);
void		 control_dispatch_runner(int, short, void *);
void		 control_dispatch_smtp(int, short, void *);
void		 control_dispatch_parent(int, short, void *);

struct ctl_connlist	ctl_conns;

void
control_sig_handler(int sig, short event, void *p)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		control_shutdown();
		break;
	default:
		fatalx("control_sig_handler: unexpected signal");
	}
}


pid_t
control(struct smtpd *env)
{
	struct sockaddr_un	 sun;
	int			 fd;
	mode_t			 old_umask;
	pid_t			 pid;
	struct passwd		*pw;
	struct event		 ev_sigint;
	struct event		 ev_sigterm;
	struct peer		 peers [] = {
		{ PROC_QUEUE,	 control_dispatch_queue },
		{ PROC_RUNNER,	 control_dispatch_runner },
		{ PROC_SMTP,	 control_dispatch_smtp },
		{ PROC_MFA,	 control_dispatch_mfa },
		{ PROC_PARENT,	 control_dispatch_parent },
	};

	switch (pid = fork()) {
	case -1:
		fatal("control: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	purge_config(env, PURGE_EVERYTHING);

	pw = env->sc_pw;

	if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		fatal("control: socket");

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	if (strlcpy(sun.sun_path, SMTPD_SOCKET,
	    sizeof(sun.sun_path)) >= sizeof(sun.sun_path))
		fatal("control: socket name too long");

	if (connect(fd, (struct sockaddr *)&sun, sizeof(sun)) == 0)
		fatalx("control socket already listening");

	if (unlink(SMTPD_SOCKET) == -1)
		if (errno != ENOENT)
			fatal("control: cannot unlink socket");

	old_umask = umask(S_IXUSR|S_IXGRP|S_IWOTH|S_IROTH|S_IXOTH);
	if (bind(fd, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		(void)umask(old_umask);
		fatal("control: bind");
	}
	(void)umask(old_umask);

	if (chmod(SMTPD_SOCKET, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH) == -1) {
		(void)unlink(SMTPD_SOCKET);
		fatal("control: chmod");
	}

	session_socket_blockmode(fd, BM_NONBLOCK);
	control_state.fd = fd;

#ifndef DEBUG
	if (chroot(pw->pw_dir) == -1)
		fatal("control: chroot");
	if (chdir("/") == -1)
		fatal("control: chdir(\"/\")");
#else
#warning disabling privilege revocation and chroot in DEBUG MODE
#endif

	smtpd_process = PROC_CONTROL;
	setproctitle("%s", env->sc_title[smtpd_process]);

#ifndef DEBUG
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("control: cannot drop privileges");
#endif

	event_init();

	signal_set(&ev_sigint, SIGINT, control_sig_handler, env);
	signal_set(&ev_sigterm, SIGTERM, control_sig_handler, env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	TAILQ_INIT(&ctl_conns);

	config_pipes(env, peers, nitems(peers));
	config_peers(env, peers, nitems(peers));
	control_listen(env);
	event_dispatch();
	control_shutdown();

	return (0);
}

void
control_shutdown(void)
{
	log_info("control process exiting");
	_exit(0);
}

void
control_listen(struct smtpd *env)
{
	int avail = availdesc();

	if (listen(control_state.fd, CONTROL_BACKLOG) == -1)
		fatal("control_listen");
	avail--;

	event_set(&control_state.ev, control_state.fd, EV_READ|EV_PERSIST,
	    control_accept, env);
	event_add(&control_state.ev, NULL);

	/* guarantee 2 fds to each accepted client */
	if ((env->sc_maxconn = avail / 2) < 1)
		fatalx("control_listen: fd starvation");
}

void
control_cleanup(void)
{
	(void)unlink(SMTPD_SOCKET);
}

/* ARGSUSED */
void
control_accept(int listenfd, short event, void *arg)
{
	int			 connfd;
	socklen_t		 len;
	struct sockaddr_un	 sun;
	struct ctl_conn		*c;
	struct smtpd		*env = arg;

	len = sizeof(sun);
	if ((connfd = accept(listenfd, (struct sockaddr *)&sun, &len)) == -1) {
		if (errno == EINTR || errno == ECONNABORTED)
			return;
		fatal("control_accept: accept");
	}

	session_socket_blockmode(connfd, BM_NONBLOCK);

	if ((c = calloc(1, sizeof(*c))) == NULL)
		fatal(NULL);
	imsg_init(&c->iev.ibuf, connfd);
	c->iev.handler = control_dispatch_ext;
	c->iev.events = EV_READ;
	c->iev.data = env;
	event_set(&c->iev.ev, c->iev.ibuf.fd, c->iev.events,
	    c->iev.handler, env);
	event_add(&c->iev.ev, NULL);
	TAILQ_INSERT_TAIL(&ctl_conns, c, entry);

	env->stats->control.sessions++;
	env->stats->control.sessions_active++;

	if (env->stats->control.sessions_active >= env->sc_maxconn) {
		log_warnx("ctl client limit hit, disabling new connections");
		event_del(&control_state.ev);
	}
}

struct ctl_conn *
control_connbyfd(int fd)
{
	struct ctl_conn	*c;

	for (c = TAILQ_FIRST(&ctl_conns); c != NULL && c->iev.ibuf.fd != fd;
	    c = TAILQ_NEXT(c, entry))
		;	/* nothing */

	return (c);
}

void
control_close(struct smtpd *env, int fd)
{
	struct ctl_conn	*c;

	if ((c = control_connbyfd(fd)) == NULL) {
		log_warn("control_close: fd %d: not found", fd);
		return;
	}
	TAILQ_REMOVE(&ctl_conns, c, entry);
	event_del(&c->iev.ev);
	imsg_clear(&c->iev.ibuf);
	close(fd);
	free(c);

	env->stats->control.sessions_active--;

	if (!event_pending(&control_state.ev, EV_READ, NULL) &&
	    env->stats->control.sessions_active < env->sc_maxconn) {
		log_warnx("re-enabling ctl connections");
		event_add(&control_state.ev, NULL);
	}
}

/* ARGSUSED */
void
control_dispatch_ext(int fd, short event, void *arg)
{
	struct ctl_conn		*c;
	struct smtpd		*env = arg;
	struct imsg		 imsg;
	int			 n;
	uid_t			 euid;
	gid_t			 egid;

	if (getpeereid(fd, &euid, &egid) == -1)
		fatal("getpeereid");

	if ((c = control_connbyfd(fd)) == NULL) {
		log_warn("control_dispatch_ext: fd %d: not found", fd);
		return;
	}

	if (event & EV_READ) {
		if ((n = imsg_read(&c->iev.ibuf)) == -1 || n == 0) {
			control_close(env, fd);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&c->iev.ibuf.w) < 0) {
			control_close(env, fd);
			return;
		}
	}

	for (;;) {
		if ((n = imsg_get(&c->iev.ibuf, &imsg)) == -1) {
			control_close(env, fd);
			return;
		}

		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_SMTP_ENQUEUE:
			if (env->sc_flags & (SMTPD_SMTP_PAUSED |
			    SMTPD_CONFIGURING | SMTPD_EXITING)) {
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL, 0, 0, -1,
					NULL, 0);
				break;
			}
			imsg_compose_event(env->sc_ievs[PROC_SMTP],
			    IMSG_SMTP_ENQUEUE, fd, 0, -1, &euid, sizeof(euid));
			break;
		case IMSG_STATS:
			if (euid)
				goto badcred;
			imsg_compose_event(&c->iev, IMSG_STATS, 0, 0, -1,
			    env->stats, sizeof(struct stats));
			break;
		case IMSG_RUNNER_SCHEDULE: {
			struct sched *s = imsg.data;

			if (euid)
				goto badcred;
	
			if (IMSG_DATA_SIZE(&imsg) != sizeof(*s))
				goto badcred;

			s->fd = fd;

			if (! valid_message_id(s->mid) && ! valid_message_uid(s->mid)) {
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL, 0, 0, -1,
				    NULL, 0);
				break;
			}

			imsg_compose_event(env->sc_ievs[PROC_RUNNER], IMSG_RUNNER_SCHEDULE, 0, 0, -1, s, sizeof(*s));
			break;
		}
/*
		case IMSG_CONF_RELOAD: {
			struct reload r;

			log_debug("received reload request");

			if (euid)
				goto badcred;

			if (env->sc_flags & SMTPD_CONFIGURING) {
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL, 0, 0, -1,
					NULL, 0);
				break;
			}
			env->sc_flags |= SMTPD_CONFIGURING;

			r.fd = fd;
			imsg_compose_event(env->sc_ievs[PROC_PARENT], IMSG_CONF_RELOAD, 0, 0, -1, &r, sizeof(r));
			break;
		}
*/
		case IMSG_CTL_SHUTDOWN:
			/* NEEDS_FIX */
			log_debug("received shutdown request");

			if (euid)
				goto badcred;

			if (env->sc_flags & SMTPD_EXITING) {
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL, 0, 0, -1,
					NULL, 0);
				break;
			}
			env->sc_flags |= SMTPD_EXITING;
			imsg_compose_event(&c->iev, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
			break;
		case IMSG_MDA_PAUSE:
			if (euid)
				goto badcred;

			if (env->sc_flags & SMTPD_MDA_PAUSED) {
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL, 0, 0, -1,
					NULL, 0);
				break;
			}
			env->sc_flags |= SMTPD_MDA_PAUSED;
			imsg_compose_event(env->sc_ievs[PROC_RUNNER], IMSG_MDA_PAUSE,
			    0, 0, -1, NULL, 0);
			imsg_compose_event(&c->iev, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
			break;
		case IMSG_MTA_PAUSE:
			if (euid)
				goto badcred;

			if (env->sc_flags & SMTPD_MTA_PAUSED) {
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL, 0, 0, -1,
					NULL, 0);
				break;
			}
			env->sc_flags |= SMTPD_MTA_PAUSED;
			imsg_compose_event(env->sc_ievs[PROC_RUNNER], IMSG_MTA_PAUSE,
			    0, 0, -1, NULL, 0);
			imsg_compose_event(&c->iev, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
			break;
		case IMSG_SMTP_PAUSE:
			if (euid)
				goto badcred;

			if (env->sc_flags & SMTPD_SMTP_PAUSED) {
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL, 0, 0, -1,
					NULL, 0);
				break;
			}
			env->sc_flags |= SMTPD_SMTP_PAUSED;
			imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_SMTP_PAUSE,			
			    0, 0, -1, NULL, 0);
			imsg_compose_event(&c->iev, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
			break;
		case IMSG_MDA_RESUME:
			if (euid)
				goto badcred;

			if (! (env->sc_flags & SMTPD_MDA_PAUSED)) {
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL, 0, 0, -1,
					NULL, 0);
				break;
			}
			env->sc_flags &= ~SMTPD_MDA_PAUSED;
			imsg_compose_event(env->sc_ievs[PROC_RUNNER], IMSG_MDA_RESUME,
			    0, 0, -1, NULL, 0);
			imsg_compose_event(&c->iev, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
			break;
		case IMSG_MTA_RESUME:
			if (euid)
				goto badcred;

			if (!(env->sc_flags & SMTPD_MTA_PAUSED)) {
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL, 0, 0, -1,
					NULL, 0);
				break;
			}
			env->sc_flags &= ~SMTPD_MTA_PAUSED;
			imsg_compose_event(env->sc_ievs[PROC_RUNNER], IMSG_MTA_RESUME,
			    0, 0, -1, NULL, 0);
			imsg_compose_event(&c->iev, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
			break;
		case IMSG_SMTP_RESUME:
			if (euid)
				goto badcred;

			if (!(env->sc_flags & SMTPD_SMTP_PAUSED)) {
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL, 0, 0, -1,
					NULL, 0);
				break;
			}
			env->sc_flags &= ~SMTPD_SMTP_PAUSED;
			imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_SMTP_RESUME,
			    0, 0, -1, NULL, 0);
			imsg_compose_event(&c->iev, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
			break;
		default:
			log_debug("control_dispatch_ext: "
			    "error handling imsg %d", imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
		continue;

badcred:
		imsg_compose_event(&c->iev, IMSG_CTL_FAIL, 0, 0, -1,
		    NULL, 0);
	}

	imsg_event_add(&c->iev);
}

void
control_dispatch_parent(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_PARENT];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("control_dispatch_parent: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_CONF_RELOAD: {
			struct reload *r = imsg.data;
			struct ctl_conn	*c;

			IMSG_SIZE_CHECK(r);

			env->sc_flags &= ~SMTPD_CONFIGURING;
			if ((c = control_connbyfd(r->fd)) == NULL) {
				log_warn("control_dispatch_parent: fd %d not found", r->fd);
				return;
			}

			if (r->ret)
				imsg_compose_event(&c->iev, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
			else
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL, 0, 0, -1, NULL, 0);
			break;
		}
		default:
			log_warnx("control_dispatch_parent: got imsg %d",
			    imsg.hdr.type);
			fatalx("control_dispatch_parent: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
control_dispatch_lka(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_LKA];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("control_dispatch_lka: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_warnx("control_dispatch_lka: got imsg %d",
			    imsg.hdr.type);
			fatalx("control_dispatch_lka: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
control_dispatch_mfa(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_MFA];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("control_dispatch_mfa: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_warnx("control_dispatch_mfa: got imsg %d",
			    imsg.hdr.type);
			fatalx("control_dispatch_mfa: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
control_dispatch_queue(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_QUEUE];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("control_dispatch_queue: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_warnx("control_dispatch_queue: got imsg %d",
			    imsg.hdr.type);
			fatalx("control_dispatch_queue: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
control_dispatch_runner(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_RUNNER];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("control_dispatch_runner: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_RUNNER_SCHEDULE: {
			struct sched	*s = imsg.data;
			struct ctl_conn	*c;

			IMSG_SIZE_CHECK(s);

			if ((c = control_connbyfd(s->fd)) == NULL) {
				log_warn("control_dispatch_runner: fd %d not found", s->fd);
				imsg_free(&imsg);
				return;
			}

			if (s->ret)
				imsg_compose_event(&c->iev, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
			else
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL, 0, 0, -1, NULL, 0);
			break;
		}
		default:
			log_warnx("control_dispatch_runner: got imsg %d",
			    imsg.hdr.type);
			fatalx("control_dispatch_runner: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

void
control_dispatch_smtp(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgev		*iev;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	iev = env->sc_ievs[PROC_SMTP];
	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read_error");
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			event_loopexit(NULL);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&ibuf->w) == -1)
			fatal("msgbuf_write");
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("control_dispatch_smtp: imsg_get error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_SMTP_ENQUEUE: {
			struct ctl_conn	*c;
			int		client_fd;

			client_fd = imsg.hdr.peerid;

			if ((c = control_connbyfd(client_fd)) == NULL) {
				log_warn("control_dispatch_smtp: fd %d not found", client_fd);
				imsg_free(&imsg);
				return;
			}

			imsg_compose_event(&c->iev, IMSG_CTL_OK, 0, 0,
			    imsg.fd, NULL, 0);
			break;
		}
		default:
			log_warnx("control_dispatch_smtp: got imsg %d",
			    imsg.hdr.type);
			fatalx("control_dispatch_smtp: unexpected imsg");
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}
