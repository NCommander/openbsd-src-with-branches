/*	$OpenBSD: control.c,v 1.71 2012/08/25 10:23:11 gilles Exp $	*/

/*
 * Copyright (c) 2012 Gilles Chehade <gilles@openbsd.org>
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

#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

#define CONTROL_BACKLOG 5

/* control specific headers */
struct {
	struct event		 ev;
	int			 fd;
} control_state;

void		 control_imsg(struct imsgev *, struct imsg *);
__dead void	 control_shutdown(void);
int		 control_init(void);
void		 control_listen(void);
void		 control_cleanup(void);
void		 control_accept(int, short, void *);
struct ctl_conn	*control_connbyfd(int);
void		 control_close(int);
void		 control_sig_handler(int, short, void *);
void		 control_dispatch_ext(int, short, void *);

struct ctl_connlist	ctl_conns;

static struct stat_backend *stat_backend = NULL;
extern const char *backend_stat;

#define	CONTROL_FD_RESERVE	5

void
control_imsg(struct imsgev *iev, struct imsg *imsg)
{
	struct ctl_conn	       *c;
	char		       *key;
	struct stat_value	val;

	log_imsg(PROC_CONTROL, iev->proc, imsg);

	if (iev->proc == PROC_SMTP) {
		switch (imsg->hdr.type) {
		case IMSG_SMTP_ENQUEUE:
			c = control_connbyfd(imsg->hdr.peerid);
			if (c == NULL)
				return;
			imsg_compose_event(&c->iev, IMSG_CTL_OK, 0, 0,
			    imsg->fd, NULL, 0);
			return;
		}
	}

	switch (imsg->hdr.type) {
	case IMSG_STAT_INCREMENT:
		memmove(&val, imsg->data, sizeof (val));
		key = (char*)imsg->data + sizeof (val);
		if (stat_backend)
			stat_backend->increment(key, val.u.counter);
		return;
	case IMSG_STAT_DECREMENT:
		memmove(&val, imsg->data, sizeof (val));
		key = (char*)imsg->data + sizeof (val);
		if (stat_backend)
			stat_backend->decrement(key, val.u.counter);
		return;
	case IMSG_STAT_SET:
		memmove(&val, imsg->data, sizeof (val));
		key = (char*)imsg->data + sizeof (val);
		if (stat_backend)
			stat_backend->set(key, &val);
		return;
	}

	errx(1, "control_imsg: unexpected %s imsg",
	    imsg_to_str(imsg->hdr.type));
}

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
control(void)
{
	struct sockaddr_un	 sun;
	int			 fd;
	mode_t			 old_umask;
	pid_t			 pid;
	struct passwd		*pw;
	struct event		 ev_sigint;
	struct event		 ev_sigterm;
	struct peer		 peers [] = {
		{ PROC_SCHEDULER,	imsg_dispatch },
		{ PROC_QUEUE,		imsg_dispatch },
		{ PROC_SMTP,		imsg_dispatch },
		{ PROC_MFA,		imsg_dispatch },
		{ PROC_PARENT,		imsg_dispatch },
		{ PROC_LKA,		imsg_dispatch },
		{ PROC_MDA,		imsg_dispatch },
		{ PROC_MTA,		imsg_dispatch }
	};

	switch (pid = fork()) {
	case -1:
		fatal("control: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	purge_config(PURGE_EVERYTHING);

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

	stat_backend = env->sc_stat;
	stat_backend->init();

	if (chroot(pw->pw_dir) == -1)
		fatal("control: chroot");
	if (chdir("/") == -1)
		fatal("control: chdir(\"/\")");

	smtpd_process = PROC_CONTROL;
	setproctitle("%s", env->sc_title[smtpd_process]);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("control: cannot drop privileges");

	imsg_callback = control_imsg;
	event_init();

	signal_set(&ev_sigint, SIGINT, control_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, control_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	TAILQ_INIT(&ctl_conns);

	config_pipes(peers, nitems(peers));
	config_peers(peers, nitems(peers));
	control_listen();

	if (event_dispatch() < 0)
		fatal("event_dispatch");
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
control_listen(void)
{
	if (listen(control_state.fd, CONTROL_BACKLOG) == -1)
		fatal("control_listen");

	event_set(&control_state.ev, control_state.fd, EV_READ|EV_PERSIST,
	    control_accept, NULL);
	event_add(&control_state.ev, NULL);
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

	if (getdtablesize() - getdtablecount() < CONTROL_FD_RESERVE)
		goto pause;

	len = sizeof(sun);
	if ((connfd = accept(listenfd, (struct sockaddr *)&sun, &len)) == -1) {
		if (errno == ENFILE || errno == EMFILE)
			goto pause;
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
	event_set(&c->iev.ev, c->iev.ibuf.fd, c->iev.events,
	    c->iev.handler, NULL);
	event_add(&c->iev.ev, NULL);
	TAILQ_INSERT_TAIL(&ctl_conns, c, entry);

	stat_backend->increment("control.session", 1);
	return;

pause:
	log_warnx("ctl client limit hit, disabling new connections");
	event_del(&control_state.ev);
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
control_close(int fd)
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

	stat_backend->decrement("control.session", 1);

	if (getdtablesize() - getdtablecount() < CONTROL_FD_RESERVE)
		return;

	if (!event_pending(&control_state.ev, EV_READ, NULL)) {
		log_warnx("re-enabling ctl connections");
		event_add(&control_state.ev, NULL);
	}
}

/* ARGSUSED */
void
control_dispatch_ext(int fd, short event, void *arg)
{
	struct ctl_conn		*c;
	struct imsg		 imsg;
	int			 n, verbose;
	uid_t			 euid;
	gid_t			 egid;
	uint64_t		 id;
	struct stat_kv		*kvp;
	char			*key;
	struct stat_value      	 val;


	if (getpeereid(fd, &euid, &egid) == -1)
		fatal("getpeereid");

	if ((c = control_connbyfd(fd)) == NULL) {
		log_warn("control_dispatch_ext: fd %d: not found", fd);
		return;
	}

	if (event & EV_READ) {
		if ((n = imsg_read(&c->iev.ibuf)) == -1 || n == 0) {
			control_close(fd);
			return;
		}
	}

	if (event & EV_WRITE) {
		if (msgbuf_write(&c->iev.ibuf.w) < 0) {
			control_close(fd);
			return;
		}
	}

	for (;;) {
		if ((n = imsg_get(&c->iev.ibuf, &imsg)) == -1) {
			control_close(fd);
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
			imsg_compose_event(&c->iev, IMSG_STATS, 0, 0, -1, NULL, 0);
			break;

		case IMSG_STATS_GET:
			if (euid)
				goto badcred;
			kvp = imsg.data;
			if (! stat_backend->iter(&kvp->iter, &key, &val))
				kvp->iter = NULL;
			else {
				strlcpy(kvp->key, key, sizeof kvp->key);
				kvp->val = val;
			}
			imsg_compose_event(&c->iev, IMSG_STATS_GET, 0, 0, -1,
			    kvp, sizeof *kvp);
			break;

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

		case IMSG_CTL_VERBOSE:
			if (euid)
				goto badcred;

			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(verbose))
				goto badcred;

			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_verbose(verbose);
			imsg_compose_event(env->sc_ievs[PROC_PARENT], IMSG_CTL_VERBOSE,
			    0, 0, -1, &verbose, sizeof(verbose));
			imsg_compose_event(&c->iev, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
			break;

		case IMSG_QUEUE_PAUSE_MDA:
			if (euid)
				goto badcred;

			if (env->sc_flags & SMTPD_MDA_PAUSED) {
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL, 0, 0, -1,
					NULL, 0);
				break;
			}
			log_info("mda paused");
			env->sc_flags |= SMTPD_MDA_PAUSED;
			imsg_compose_event(env->sc_ievs[PROC_QUEUE],
			    IMSG_QUEUE_PAUSE_MDA, 0, 0, -1, NULL, 0);
			imsg_compose_event(&c->iev, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
			break;

		case IMSG_QUEUE_PAUSE_MTA:
			if (euid)
				goto badcred;

			if (env->sc_flags & SMTPD_MTA_PAUSED) {
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL, 0, 0, -1,
					NULL, 0);
				break;
			}
			log_info("mta paused");
			env->sc_flags |= SMTPD_MTA_PAUSED;
			imsg_compose_event(env->sc_ievs[PROC_QUEUE],
			    IMSG_QUEUE_PAUSE_MTA, 0, 0, -1, NULL, 0);
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
			log_info("smtp paused");
			env->sc_flags |= SMTPD_SMTP_PAUSED;
			imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_SMTP_PAUSE,			
			    0, 0, -1, NULL, 0);
			imsg_compose_event(&c->iev, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
			break;

		case IMSG_QUEUE_RESUME_MDA:
			if (euid)
				goto badcred;

			if (! (env->sc_flags & SMTPD_MDA_PAUSED)) {
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL, 0, 0, -1,
					NULL, 0);
				break;
			}
			log_info("mda resumed");
			env->sc_flags &= ~SMTPD_MDA_PAUSED;
			imsg_compose_event(env->sc_ievs[PROC_QUEUE],
			    IMSG_QUEUE_RESUME_MDA, 0, 0, -1, NULL, 0);
			imsg_compose_event(&c->iev, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
			break;

		case IMSG_QUEUE_RESUME_MTA:
			if (euid)
				goto badcred;

			if (!(env->sc_flags & SMTPD_MTA_PAUSED)) {
				imsg_compose_event(&c->iev, IMSG_CTL_FAIL, 0, 0, -1,
					NULL, 0);
				break;
			}
			log_info("mta resumed");
			env->sc_flags &= ~SMTPD_MTA_PAUSED;
			imsg_compose_event(env->sc_ievs[PROC_QUEUE],
			    IMSG_QUEUE_RESUME_MTA, 0, 0, -1, NULL, 0);
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
			log_info("smtp resumed");
			env->sc_flags &= ~SMTPD_SMTP_PAUSED;
			imsg_compose_event(env->sc_ievs[PROC_SMTP], IMSG_SMTP_RESUME,
			    0, 0, -1, NULL, 0);
			imsg_compose_event(&c->iev, IMSG_CTL_OK, 0, 0, -1, NULL, 0);
			break;

		case IMSG_SCHEDULER_SCHEDULE:
			if (euid)
				goto badcred;

			id = *(uint64_t *)imsg.data;
			imsg_compose_event(env->sc_ievs[PROC_SCHEDULER],
			    IMSG_SCHEDULER_SCHEDULE, 0, 0, -1, &id, sizeof id);
			imsg_compose_event(&c->iev, IMSG_CTL_OK, 0, 0, -1,
			    NULL, 0);
			break;

		case IMSG_SCHEDULER_REMOVE:
			if (euid)
				goto badcred;

			id = *(uint64_t *)imsg.data;
			imsg_compose_event(env->sc_ievs[PROC_SCHEDULER],
			    IMSG_SCHEDULER_REMOVE, 0, 0, -1, &id, sizeof id);
			imsg_compose_event(&c->iev, IMSG_CTL_OK, 0, 0, -1,
			    NULL, 0);
			break;

		default:
			log_debug("control_dispatch_ext: "
			    "error handling %s imsg",
			    imsg_to_str(imsg.hdr.type));
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
