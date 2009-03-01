/*	$OpenBSD: queue.c,v 1.56 2009/02/22 19:07:33 chl Exp $	*/

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
#include <sys/stat.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <libgen.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"

__dead void	queue_shutdown(void);
void		queue_sig_handler(int, short, void *);
void		queue_dispatch_control(int, short, void *);
void		queue_dispatch_smtp(int, short, void *);
void		queue_dispatch_mda(int, short, void *);
void		queue_dispatch_mta(int, short, void *);
void		queue_dispatch_lka(int, short, void *);
void		queue_dispatch_runner(int, short, void *);
void		queue_setup_events(struct smtpd *);
void		queue_disable_events(struct smtpd *);
void		queue_purge(char *);

int		queue_create_layout_message(char *, char *);
void		queue_delete_layout_message(char *, char *);
int		queue_record_layout_envelope(char *, struct message *);
int		queue_remove_layout_envelope(char *, struct message *);
int		queue_commit_layout_message(char *, struct message *);
int		queue_open_layout_messagefile(char *, struct message *);

struct s_queue	s_queue;

void
queue_sig_handler(int sig, short event, void *p)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		queue_shutdown();
		break;
	default:
		fatalx("queue_sig_handler: unexpected signal");
	}
}

void
queue_dispatch_control(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_CONTROL];
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
			fatal("queue_dispatch_control: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_QUEUE_CREATE_MESSAGE: {
			struct message		*messagep;
			struct submit_status	 ss;

			log_debug("mfa_dispatch_control: creating message file");
			messagep = imsg.data;

			ss.id = messagep->session_id;
			ss.code = 250;
			bzero(ss.u.msgid, MAX_ID_SIZE);

			if (! enqueue_create_layout(ss.u.msgid))
				ss.code = 421;

			imsg_compose(ibuf, IMSG_QUEUE_CREATE_MESSAGE, 0, 0, -1,
			    &ss, sizeof(ss));

			break;
		}
		case IMSG_QUEUE_MESSAGE_FILE: {
			int fd;
			struct submit_status ss;
			struct message *messagep;

			messagep = imsg.data;
			ss.msg = *messagep;
			ss.id = messagep->session_id;
			ss.code = 250;
			fd = enqueue_open_messagefile(messagep);
			if (fd == -1) 
				ss.code = 421;

			imsg_compose(ibuf, IMSG_QUEUE_MESSAGE_FILE, 0, 0, fd, &ss,
			    sizeof(ss));

			break;
		}
		case IMSG_QUEUE_COMMIT_MESSAGE: {
			struct message		*messagep;
			struct submit_status	 ss;

			messagep = imsg.data;
			ss.id = messagep->session_id;

			ss.code = 250;
			if (enqueue_commit_message(messagep))
				s_queue.inserts_local++;
			else
				ss.code = 421;

			imsg_compose(ibuf, IMSG_QUEUE_COMMIT_MESSAGE, 0, 0, -1,
			    &ss, sizeof(ss));

			break;
		}
		case IMSG_STATS: {
			struct stats *s;

			s = imsg.data;
			s->u.queue = s_queue;
			imsg_compose(ibuf, IMSG_STATS, 0, 0, -1, s, sizeof(*s));
			break;
		}
		default:
			log_debug("queue_dispatch_control: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
queue_dispatch_smtp(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_SMTP];
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
			fatal("queue_dispatch_smtp: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_QUEUE_CREATE_MESSAGE: {
			struct message		*messagep;
			struct submit_status	 ss;

			log_debug("mfa_dispatch_smtp: creating message file");
			messagep = imsg.data;
			ss.id = messagep->session_id;
			ss.code = 250;
			bzero(ss.u.msgid, MAX_ID_SIZE);

			if (! queue_create_incoming_layout(ss.u.msgid))
				ss.code = 421;

			imsg_compose(ibuf, IMSG_QUEUE_CREATE_MESSAGE, 0, 0, -1,
			    &ss, sizeof(ss));
			break;
		}
		case IMSG_QUEUE_REMOVE_MESSAGE: {
			struct message		*messagep;

			messagep = imsg.data;
			queue_delete_incoming_message(messagep->message_id);
			break;
		}
		case IMSG_QUEUE_COMMIT_MESSAGE: {
			struct message		*messagep;
			struct submit_status	 ss;

			messagep = imsg.data;
			ss.id = messagep->session_id;

			if (queue_commit_incoming_message(messagep))
				s_queue.inserts_remote++;
			else
				ss.code = 421;

			imsg_compose(ibuf, IMSG_QUEUE_COMMIT_MESSAGE, 0, 0, -1,
			    &ss, sizeof(ss));

			break;
		}
		case IMSG_QUEUE_MESSAGE_FILE: {
			struct message		*messagep;
			struct submit_status	 ss;
			int fd;

			messagep = imsg.data;
			ss.id = messagep->session_id;

			fd = queue_open_incoming_message_file(messagep);
			if (fd == -1)
				ss.code = 421;

			imsg_compose(ibuf, IMSG_QUEUE_MESSAGE_FILE, 0, 0, fd,
			    &ss, sizeof(ss));
			break;
		}
		default:
			log_debug("queue_dispatch_smtp: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
queue_dispatch_mda(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_MDA];
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
			fatal("queue_dispatch_mda: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {

		case IMSG_QUEUE_MESSAGE_UPDATE: {
			imsg_compose(env->sc_ibufs[PROC_RUNNER], IMSG_RUNNER_UPDATE_ENVELOPE,
			    0, 0, -1, imsg.data, sizeof(struct message));
			break;
		}

		default:
			log_debug("queue_dispatch_mda: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
queue_dispatch_mta(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_MTA];
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
			fatal("queue_dispatch_mda: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {

		case IMSG_QUEUE_MESSAGE_FD: {
			int fd;
			struct batch *batchp;

			batchp = imsg.data;
			fd = queue_open_message_file(batchp->message_id);
			imsg_compose(ibuf,  IMSG_QUEUE_MESSAGE_FD, 0, 0, fd, batchp,
			    sizeof(*batchp));
			break;
		}

		case IMSG_QUEUE_MESSAGE_UPDATE: {
			imsg_compose(env->sc_ibufs[PROC_RUNNER], IMSG_RUNNER_UPDATE_ENVELOPE,
			    0, 0, -1, imsg.data, sizeof(struct message));
			break;
		}

		default:
			log_debug("queue_dispatch_mda: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
queue_dispatch_lka(int sig, short event, void *p)
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
			fatal("queue_dispatch_lka: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {

		case IMSG_QUEUE_SUBMIT_ENVELOPE: {
			struct message		*messagep;
			struct submit_status	 ss;
			int (*f)(struct message *);
			enum smtp_proc_type peer;

			messagep = imsg.data;
			messagep->id = queue_generate_id();
			ss.id = messagep->session_id;

			if (IS_MAILBOX(messagep->recipient.rule.r_action) ||
			    IS_EXT(messagep->recipient.rule.r_action))
				messagep->type = T_MDA_MESSAGE;
			else
				messagep->type = T_MTA_MESSAGE;

			/* Write to disk */
			if (messagep->flags & F_MESSAGE_ENQUEUED) {
				f = enqueue_record_envelope;
				peer = PROC_CONTROL;
			}
			else {
				f = queue_record_incoming_envelope;
				peer = PROC_SMTP;
			}

			if (! f(messagep)) {
				ss.code = 421;
				imsg_compose(env->sc_ibufs[peer], IMSG_QUEUE_TEMPFAIL,
				    0, 0, -1, &ss, sizeof(ss));
			}

			break;
		}

		case IMSG_QUEUE_COMMIT_ENVELOPES: {
			struct message		*messagep;
			struct submit_status	 ss;
			enum smtp_proc_type	 peer;

			messagep = imsg.data;
			ss.id = messagep->session_id;
			ss.code = 250;

			if (messagep->flags & F_MESSAGE_ENQUEUED)
				peer = PROC_CONTROL;
			else
				peer = PROC_SMTP;

			imsg_compose(env->sc_ibufs[peer], IMSG_QUEUE_COMMIT_ENVELOPES,
			    0, 0, -1, &ss, sizeof(ss));

			break;
		}

		default:
			log_debug("queue_dispatch_lka: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
queue_dispatch_runner(int sig, short event, void *p)
{
	struct smtpd		*env = p;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	ssize_t			 n;

	ibuf = env->sc_ibufs[PROC_RUNNER];
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
			fatal("queue_dispatch_runner: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		default:
			log_debug("queue_dispatch_runner: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
queue_shutdown(void)
{
	log_info("queue handler");
	_exit(0);
}

void
queue_setup_events(struct smtpd *env)
{
}

void
queue_disable_events(struct smtpd *env)
{
}

pid_t
queue(struct smtpd *env)
{
	pid_t		 pid;
	struct passwd	*pw;

	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	struct peer peers[] = {
		{ PROC_CONTROL,	queue_dispatch_control },
		{ PROC_SMTP,	queue_dispatch_smtp },
		{ PROC_MDA,	queue_dispatch_mda },
		{ PROC_MTA,	queue_dispatch_mta },
		{ PROC_LKA,	queue_dispatch_lka },
		{ PROC_RUNNER,	queue_dispatch_runner }
	};

	switch (pid = fork()) {
	case -1:
		fatal("queue: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	purge_config(env, PURGE_EVERYTHING);

	pw = env->sc_pw;

#ifndef DEBUG
	if (chroot(PATH_SPOOL) == -1)
		fatal("queue: chroot");
	if (chdir("/") == -1)
		fatal("queue: chdir(\"/\")");
#else
#warning disabling privilege revocation and chroot in DEBUG MODE
#endif

	setproctitle("queue handler");
	smtpd_process = PROC_QUEUE;

#ifndef DEBUG
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("queue: cannot drop privileges");
#endif

	event_init();

	signal_set(&ev_sigint, SIGINT, queue_sig_handler, env);
	signal_set(&ev_sigterm, SIGTERM, queue_sig_handler, env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	config_pipes(env, peers, 6);
	config_peers(env, peers, 6);

	queue_purge(PATH_INCOMING);
	queue_purge(PATH_ENQUEUE);

	queue_setup_events(env);
	event_dispatch();
	queue_shutdown();

	return (0);
}

int
queue_remove_batch_message(struct smtpd *env, struct batch *batchp, struct message *messagep)
{
	TAILQ_REMOVE(&batchp->messages, messagep, entry);
	bzero(messagep, sizeof(struct message));
	free(messagep);

	if (TAILQ_FIRST(&batchp->messages) == NULL) {
		SPLAY_REMOVE(batchtree, &env->batch_queue, batchp);
		bzero(batchp, sizeof(struct batch));
		free(batchp);
		return 1;
	}
	return 0;
}

struct batch *
batch_by_id(struct smtpd *env, u_int64_t id)
{
	struct batch lookup;

	lookup.id = id;
	return SPLAY_FIND(batchtree, &env->batch_queue, &lookup);
}


struct message *
message_by_id(struct smtpd *env, struct batch *batchp, u_int64_t id)
{
	struct message *messagep;

	if (batchp != NULL) {
		TAILQ_FOREACH(messagep, &batchp->messages, entry) {
			if (messagep->id == id)
				break;
		}
		return messagep;
	}

	SPLAY_FOREACH(batchp, batchtree, &env->batch_queue) {
		TAILQ_FOREACH(messagep, &batchp->messages, entry) {
			if (messagep->id == id)
				return messagep;
		}
	}
	return NULL;
}

void
queue_purge(char *queuepath)
{
	char		 path[MAXPATHLEN];
	struct qwalk	*q;

	q = qwalk_new(queuepath);

	while (qwalk(q, path))
		queue_delete_layout_message(queuepath, basename(path));

	qwalk_close(q);
}
