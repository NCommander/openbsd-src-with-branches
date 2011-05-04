/*	$OpenBSD: iscsid.c,v 1.6 2011/05/02 06:32:56 claudio Exp $ */

/*
 * Copyright (c) 2009 Claudio Jeker <claudio@openbsd.org>
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
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <err.h>
#include <event.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "iscsid.h"
#include "log.h"

void		main_sig_handler(int, short, void *);
__dead void	usage(void);
void		shutdown_cb(int, short, void *);

struct initiator *initiator;
struct event exit_ev;
int exit_rounds;
#define ISCSI_EXIT_WAIT 5

const struct session_params	iscsi_sess_defaults = {
	.MaxBurstLength = 262144,
	.FirstBurstLength = 65536,
	.DefaultTime2Wait = 2,
	.DefaultTime2Retain = 20,
	.MaxOutstandingR2T = 1,
	.MaxConnections = 1,
	.InitialR2T = 1,
	.ImmediateData = 1,
	.DataPDUInOrder = 1,
	.DataSequenceInOrder = 1,
	.ErrorRecoveryLevel = 0
};

const struct connection_params	iscsi_conn_defaults = {
	.MaxRecvDataSegmentLength = 8192
};

int
main(int argc, char *argv[])
{
	struct event ev_sigint, ev_sigterm, ev_sighup;
	struct passwd *pw;
	char *ctrlsock = ISCSID_CONTROL;
	char *vscsidev = ISCSID_DEVICE;
	int ch, debug = 0;

	log_init(1);    /* log to stderr until daemonized */

	while ((ch = getopt(argc, argv, "dn:s:")) != -1) {
		switch (ch) {
		case 'd':
			debug = 1;
			break;
		case 'n':
			vscsidev = optarg;
			break;
		case 's':
			ctrlsock = optarg;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	argc -= optind;
	argv += optind;

	if (argc > 0)
		usage();

	/* check for root privileges  */
	if (geteuid())
		errx(1, "need root privileges");

	log_init(debug);
	if (!debug)
		daemon(1, 0);
	log_info("startup");

	event_init();
	vscsi_open(vscsidev);
	if (control_init(ctrlsock) == -1)
		fatalx("control socket setup failed");

	/* chroot and drop to iscsid user */
	if ((pw = getpwnam(ISCSID_USER)) == NULL)
		errx(1, "unknown user %s", ISCSID_USER);

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");
	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("can't drop privileges");

	/* setup signal handler */
	signal_set(&ev_sigint, SIGINT, main_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, main_sig_handler, NULL);
	signal_set(&ev_sighup, SIGHUP, main_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal_add(&ev_sighup, NULL);
	signal(SIGPIPE, SIG_IGN);

	if (control_listen() == -1)
		fatalx("control socket listen failed");

	initiator = initiator_init();

	event_dispatch();

	/* CLEANUP XXX */
	control_cleanup(ctrlsock);
	initiator_cleanup(initiator);
	log_info("exiting.");
	return 0;
}

/* ARGSUSED */
void
main_sig_handler(int sig, short event, void *arg)
{
	struct timeval tv;

	/* signal handler rules don't apply, libevent decouples for us */
	switch (sig) {
	case SIGTERM:
	case SIGINT:
	case SIGHUP:
		initiator_shutdown(initiator);
		evtimer_set(&exit_ev, shutdown_cb, NULL);
		timerclear(&tv);
		if (evtimer_add(&exit_ev, &tv) == -1)
			fatal("main_sig_handler");
		break;
	default:
		fatalx("unexpected signal");
		/* NOTREACHED */
	}
}

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-d] [-n device] [-s socket]\n",
	    __progname);
	exit(1);
}

void
iscsid_ctrl_dispatch(void *ch, struct pdu *pdu)
{
	struct ctrlmsghdr *cmh;
	struct initiator_config *ic;
	struct session_config *sc;
	struct session *s;
	int *valp;

	cmh = pdu_getbuf(pdu, NULL, 0);
	if (cmh == NULL)
		goto done;

	switch (cmh->type) {
	case CTRL_INITIATOR_CONFIG:
		if (cmh->len[0] != sizeof(*ic)) {
			log_warnx("CTRL_INITIATOR_CONFIG bad size");
			control_compose(ch, CTRL_FAILURE, NULL, 0);
			break;
		}
		ic = pdu_getbuf(pdu, NULL, 1);
		bcopy(ic, &initiator->config, sizeof(initiator->config));
		control_compose(ch, CTRL_SUCCESS, NULL, 0);
		break;
	case CTRL_SESSION_CONFIG:
		if (cmh->len[0] != sizeof(*sc)) {
			log_warnx("CTRL_SESSION_CONFIG bad size");
			control_compose(ch, CTRL_FAILURE, NULL, 0);
			break;
		}
		sc = pdu_getbuf(pdu, NULL, 1);
		if (cmh->len[1])
			sc->TargetName = pdu_getbuf(pdu, NULL, 2);
		else if (sc->SessionType != SESSION_TYPE_DISCOVERY) {
			control_compose(ch, CTRL_FAILURE, NULL, 0);
			goto done;
		} else
			sc->TargetName = NULL;
		if (cmh->len[2])
			sc->InitiatorName = pdu_getbuf(pdu, NULL, 3);
		else
			sc->InitiatorName = NULL;

		s = session_find(initiator, sc->SessionName);
		if (s == NULL) {
			s = session_new(initiator, sc->SessionType);
			if (s == NULL) {
				control_compose(ch, CTRL_FAILURE, NULL, 0);
				goto done;
			}
		}

		session_config(s, sc);
		if (s->state == SESS_INIT)
			session_fsm(s, SESS_EV_START, NULL);

		control_compose(ch, CTRL_SUCCESS, NULL, 0);
		break;
	case CTRL_LOG_VERBOSE:
		if (cmh->len[0] != sizeof(int)) {
			log_warnx("CTRL_LOG_VERBOSE bad size");
			control_compose(ch, CTRL_FAILURE, NULL, 0);
			break;
		}
		valp = pdu_getbuf(pdu, NULL, 1);
		log_verbose(*valp);
		control_compose(ch, CTRL_SUCCESS, NULL, 0);
		break;
	default:
		log_warnx("unknown control message type %d", cmh->type);
		control_compose(ch, CTRL_FAILURE, NULL, 0);
		break;
	}

done:
	pdu_free(pdu);
}

void
shutdown_cb(int fd, short event, void *arg)
{
	struct timeval tv;

	if (exit_rounds++ >= ISCSI_EXIT_WAIT || initiator_isdown(initiator))
		event_loopexit(NULL);

	timerclear(&tv);
	tv.tv_sec = 1;

	if (evtimer_add(&exit_ev, &tv) == -1)
		fatal("shutdown_cb");
}

#define MERGE_MIN(r, a, b, v)				\
	res->v = (mine->v < his->v ? mine->v : his->v)
#define MERGE_MAX(r, a, b, v)				\
	res->v = (mine->v > his->v ? mine->v : his->v)
#define MERGE_OR(r, a, b, v)				\
	res->v = (mine->v || his->v)
#define MERGE_AND(r, a, b, v)				\
	res->v = (mine->v && his->v)

void
iscsi_merge_sess_params(struct session_params *res,
    struct session_params *mine, struct session_params *his)
{
	MERGE_MIN(res, mine, his, MaxBurstLength);
	MERGE_MIN(res, mine, his, FirstBurstLength);
	MERGE_MAX(res, mine, his, DefaultTime2Wait);
	MERGE_MIN(res, mine, his, DefaultTime2Retain);
	MERGE_MIN(res, mine, his, MaxOutstandingR2T);
	res->TargetPortalGroupTag = his->TargetPortalGroupTag;
	MERGE_MIN(res, mine, his, MaxConnections);
	MERGE_OR(res, mine, his, InitialR2T);
	MERGE_AND(res, mine, his, ImmediateData);
	MERGE_OR(res, mine, his, DataPDUInOrder);
	MERGE_OR(res, mine, his, DataSequenceInOrder);
	MERGE_MIN(res, mine, his, ErrorRecoveryLevel);

}

void
iscsi_merge_conn_params(struct connection_params *res,
    struct connection_params *mine, struct connection_params *his)
{
	res->MaxRecvDataSegmentLength = his->MaxRecvDataSegmentLength;
	/* XXX HeaderDigest and DataDigest */
}

#undef MERGE_MIN
#undef MERGE_MAX
#undef MERGE_OR
#undef MERGE_AND
