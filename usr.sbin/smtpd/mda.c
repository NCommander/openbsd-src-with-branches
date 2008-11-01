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

#include <err.h>
#include <event.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"

__dead void	mda_shutdown(void);
void		mda_sig_handler(int, short, void *);
void		mda_dispatch_parent(int, short, void *);
void		mda_dispatch_queue(int, short, void *);
void		mda_setup_events(struct smtpd *);
void		mda_disable_events(struct smtpd *);
void		mda_timeout(int, short, void *);
void		mda_remove_message(struct smtpd *, struct batch *, struct message *x);

void
mda_sig_handler(int sig, short event, void *p)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		mda_shutdown();
		break;
	default:
		fatalx("mda_sig_handler: unexpected signal");
	}
}

void
mda_dispatch_parent(int sig, short event, void *p)
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
			fatal("parent_dispatch_mda: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_MDA_MAILBOX_FILE: {
			struct batch	*batchp;
			struct message	*messagep;
			enum message_status status;

			batchp = (struct batch *)imsg.data;
			messagep = &batchp->message;
			status = messagep->status;

			batchp = batch_by_id(env, batchp->id);
			if (batchp == NULL)
				errx(1, "%s: internal inconsistency.", __func__);

			messagep = message_by_id(env, batchp, messagep->id);
			if (messagep == NULL)
				errx(1, "%s: internal inconsistency.", __func__);
			messagep->status = status;

			messagep->mboxfd = imsg_get_fd(ibuf, &imsg);
			if (messagep->mboxfd == -1) {
				mda_remove_message(env, batchp, messagep);
				break;
			}

			batchp->message = *messagep;
			imsg_compose(env->sc_ibufs[PROC_PARENT],
			    IMSG_PARENT_MESSAGE_OPEN, 0, 0, -1, batchp,
			    sizeof(struct batch));
			break;
		}

		case IMSG_MDA_MESSAGE_FILE: {
			struct batch	*batchp;
			struct message	*messagep;
			enum message_status status;
			int (*store)(struct batch *, struct message *) = store_write_message;

			batchp = (struct batch *)imsg.data;
			messagep = &batchp->message;
			status = messagep->status;

			batchp = batch_by_id(env, batchp->id);
			if (batchp == NULL)
				errx(1, "%s: internal inconsistency.", __func__);

			messagep = message_by_id(env, batchp, messagep->id);
			if (messagep == NULL)
				errx(1, "%s: internal inconsistency.", __func__);
			messagep->status = status;

			messagep->messagefd = imsg_get_fd(ibuf, &imsg);
			if (messagep->messagefd == -1) {
				if (messagep->mboxfd != -1)
					close(messagep->mboxfd);
				mda_remove_message(env, batchp, messagep);
				break;
			}

			/* If batch is a daemon message, override the default store function */
			if (batchp->type & T_DAEMON_BATCH) {
				store = store_write_daemon;
			}

			if (store_message(batchp, messagep, store)) {
				if (batchp->message.recipient.rule.r_action == A_MAILDIR)
					imsg_compose(env->sc_ibufs[PROC_PARENT],
					    IMSG_PARENT_MAILBOX_RENAME, 0, 0, -1, batchp,
					    sizeof(struct batch));
			}

			if (messagep->mboxfd != -1)
				close(messagep->mboxfd);
			if (messagep->messagefd != -1)
				close(messagep->messagefd);

			mda_remove_message(env, batchp, messagep);
			break;
		}
		default:
			log_debug("parent_dispatch_mda: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

void
mda_dispatch_queue(int sig, short event, void *p)
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
			fatal("parent_dispatch_queue: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_CREATE_BATCH: {
			struct batch	*batchp;

			batchp = calloc(1, sizeof (struct batch));
			if (batchp == NULL)
				fatal("calloc");
			*batchp = *(struct batch *)imsg.data;
			batchp->env = env;
			batchp->flags = 0;

			TAILQ_INIT(&batchp->messages);
			SPLAY_INSERT(batchtree, &env->batch_queue, batchp);
			break;
		}

		case IMSG_BATCH_APPEND: {
			struct batch	*batchp;
			struct message	*messagep;

			messagep = calloc(1, sizeof (struct message));
			if (messagep == NULL)
				fatal("calloc");

			*messagep = *(struct message *)imsg.data;
			batchp = batch_by_id(env, messagep->batch_id);
			if (batchp == NULL)
				errx(1, "%s: internal inconsistency.", __func__);

			TAILQ_INSERT_TAIL(&batchp->messages, messagep, entry);
			break;
		}

		case IMSG_BATCH_CLOSE: {
			struct batch	lookup;
			struct batch	*batchp;
			struct message	*messagep;

			lookup = *(struct batch *)imsg.data;
			batchp = batch_by_id(env, lookup.id);
			if (batchp == NULL)
				errx(1, "%s: internal inconsistency.", __func__);

			lookup = *batchp;
			TAILQ_FOREACH(messagep, &batchp->messages, entry) {
				lookup.message = *messagep;
				imsg_compose(env->sc_ibufs[PROC_PARENT],
				    IMSG_PARENT_MAILBOX_OPEN, 0, 0, -1, &lookup,
				    sizeof(struct batch));
			}

			break;
		}

		default:
			log_debug("parent_dispatch_queue: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}


void
mda_shutdown(void)
{
	log_info("mail delivery agent exiting");
	_exit(0);
}

void
mda_setup_events(struct smtpd *env)
{
	struct timeval	 tv;

	evtimer_set(&env->sc_ev, mda_timeout, env);
	tv.tv_sec = 3;
	tv.tv_usec = 0;
	evtimer_add(&env->sc_ev, &tv);
}

void
mda_disable_events(struct smtpd *env)
{
	evtimer_del(&env->sc_ev);
}

void
mda_timeout(int fd, short event, void *p)
{
	struct smtpd		*env = p;
	struct timeval		 tv;

	tv.tv_sec = 3;
	tv.tv_usec = 0;
	evtimer_add(&env->sc_ev, &tv);
}

pid_t
mda(struct smtpd *env)
{
	pid_t		 pid;
	struct passwd	*pw;

	struct event	 ev_sigint;
	struct event	 ev_sigterm;

	struct peer peers[] = {
		{ PROC_PARENT,	mda_dispatch_parent },
		{ PROC_QUEUE,	mda_dispatch_queue }
	};

	switch (pid = fork()) {
	case -1:
		fatal("mda: cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	purge_config(env, PURGE_EVERYTHING);

	pw = env->sc_pw;

#ifndef DEBUG
	if (chroot(pw->pw_dir) == -1)
		fatal("mda: chroot");
	if (chdir("/") == -1)
		fatal("mda: chdir(\"/\")");
#else
#warning disabling privilege revocation and chroot in DEBUG MODE
#endif

	setproctitle("mail delivery agent");
	smtpd_process = PROC_MDA;

#ifndef DEBUG
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("mda: cannot drop privileges");
#endif

	SPLAY_INIT(&env->batch_queue);

	event_init();

	signal_set(&ev_sigint, SIGINT, mda_sig_handler, env);
	signal_set(&ev_sigterm, SIGTERM, mda_sig_handler, env);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	config_peers(env, peers, 2);

	mda_setup_events(env);
	event_dispatch();
	mda_shutdown();

	return (0);
}

void
mda_remove_message(struct smtpd *env, struct batch *batchp, struct message *messagep)
{
	imsg_compose(env->sc_ibufs[PROC_QUEUE], IMSG_QUEUE_MESSAGE_UPDATE, 0, 0,
	    -1, messagep, sizeof (struct message));

	queue_remove_batch_message(env, batchp, messagep);
}
