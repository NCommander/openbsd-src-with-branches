/*	$OpenBSD: rde.c,v 1.1 2006/06/01 14:12:20 norby Exp $ */

/*
 * Copyright (c) 2004, 2005 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2004, 2005, 2006 Esben Norby <norby@openbsd.org>
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
#include <sys/socket.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <event.h>

#include "igmp.h"
#include "dvmrp.h"
#include "dvmrpd.h"
#include "dvmrpe.h"
#include "log.h"
#include "rde.h"

void		 rde_sig_handler(int sig, short, void *);
void		 rde_shutdown(void);
void		 rde_dispatch_imsg(int, short, void *);

volatile sig_atomic_t	 rde_quit = 0;
struct dvmrpd_conf	*rdeconf = NULL;
struct imsgbuf		*ibuf_dvmrpe;
struct imsgbuf		*ibuf_main;

void
rde_sig_handler(int sig, short event, void *arg)
{
	/*
	 * signal handler rules don't apply, libevent decouples for us
	 */

	switch (sig) {
	case SIGINT:
	case SIGTERM:
		rde_shutdown();
		/* NOTREACHED */
	default:
		fatalx("unexpected signal");
	}
}

/* route decision engine */
pid_t
rde(struct dvmrpd_conf *xconf, int pipe_parent2rde[2], int pipe_dvmrpe2rde[2],
    int pipe_parent2dvmrpe[2])
{
	struct passwd		*pw;
	struct event		 ev_sigint, ev_sigterm;
	pid_t			 pid;

	switch (pid = fork()) {
	case -1:
		fatal("cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	rdeconf = xconf;

	if ((pw = getpwnam(DVMRPD_USER)) == NULL)
		fatal("getpwnam");

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	setproctitle("route decision engine");
	dvmrpd_process = PROC_RDE_ENGINE;

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("can't drop privileges");

	endpwent();

	event_init();

	/* setup signal handler */
	signal_set(&ev_sigint, SIGINT, rde_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, rde_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);

	/* setup pipes */
	close(pipe_dvmrpe2rde[0]);
	close(pipe_parent2rde[0]);
	close(pipe_parent2dvmrpe[0]);
	close(pipe_parent2dvmrpe[1]);

	if ((ibuf_dvmrpe = malloc(sizeof(struct imsgbuf))) == NULL ||
	    (ibuf_main = malloc(sizeof(struct imsgbuf))) == NULL)
		fatal(NULL);
	imsg_init(ibuf_dvmrpe, pipe_dvmrpe2rde[1], rde_dispatch_imsg);
	imsg_init(ibuf_main, pipe_parent2rde[1], rde_dispatch_imsg);

	/* setup event handler */
	ibuf_dvmrpe->events = EV_READ;
	event_set(&ibuf_dvmrpe->ev, ibuf_dvmrpe->fd, ibuf_dvmrpe->events,
	    ibuf_dvmrpe->handler, ibuf_dvmrpe);
	event_add(&ibuf_dvmrpe->ev, NULL);

	ibuf_main->events = EV_READ;
	event_set(&ibuf_main->ev, ibuf_main->fd, ibuf_main->events,
	    ibuf_main->handler, ibuf_main);
	event_add(&ibuf_main->ev, NULL);

	rt_init();
	mfc_init();

	event_dispatch();

	rde_shutdown();
	/* NOTREACHED */

	return (0);
}

void
rde_shutdown(void)
{
	struct iface	*iface;

	rt_clear();
	mfc_clear();

	LIST_FOREACH(iface, &rdeconf->iface_list, entry) {
		if_del(iface);
	}

	msgbuf_clear(&ibuf_dvmrpe->w);
	free(ibuf_dvmrpe);
	msgbuf_clear(&ibuf_main->w);
	free(ibuf_main);
	free(rdeconf);

	log_info("route decision engine exiting");
	_exit(0);
}

/* imesg */
int
rde_imsg_compose_parent(int type, pid_t pid, void *data, u_int16_t datalen)
{
	return (imsg_compose(ibuf_main, type, 0, pid, data, datalen));
}

int
rde_imsg_compose_dvmrpe(int type, u_int32_t peerid, pid_t pid, void *data,
    u_int16_t datalen)
{
	return (imsg_compose(ibuf_dvmrpe, type, peerid, pid, data, datalen));
}

void
rde_dispatch_imsg(int fd, short event, void *bula)
{
	struct mfc		 mfc;
	struct imsgbuf		*ibuf = bula;
	struct imsg		 imsg;
	struct route_report	 rr;
	int			 n, connected = 0;
	int			 i;
	struct iface		*iface;

	switch (event) {
	case EV_READ:
		if ((n = imsg_read(ibuf)) == -1)
			fatal("imsg_read error");
		if (n == 0)	/* connection closed */
			fatalx("pipe closed");
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
			fatal("rde_dispatch_imsg: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_CTL_SHOW_RIB:
			rt_dump(imsg.hdr.pid);
			imsg_compose(ibuf_dvmrpe, IMSG_CTL_END, 0, imsg.hdr.pid,
			    NULL, 0);
			break;
		case IMSG_CTL_SHOW_MFC:
			mfc_dump(imsg.hdr.pid);
			imsg_compose(ibuf_dvmrpe, IMSG_CTL_END, 0, imsg.hdr.pid,
			    NULL, 0);
			break;
		case IMSG_ROUTE_REPORT:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(rr))
				fatalx("invalid size of OE request");
			memcpy(&rr, imsg.data, sizeof(rr));

			/* directly connected networks from parent */
			if (imsg.hdr.peerid == 0) {
				connected = 1;
			}
			rt_update(rr.net, mask2prefixlen(rr.mask.s_addr),
			    rr.nexthop, rr.metric, rr.adv_rtr, rr.ifindex, 0,
			    connected);
			break;
		case IMSG_FULL_ROUTE_REPORT:
			rt_snap(imsg.hdr.peerid);
			rde_imsg_compose_dvmrpe(IMSG_FULL_ROUTE_REPORT_END,
			    imsg.hdr.peerid, 0, NULL, 0);
			break;
		case IMSG_MFC_ADD:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(mfc))
				fatalx("invalid size of OE request");
			memcpy(&mfc, imsg.data, sizeof(mfc));
#if 1
			for (i = 0; i < MAXVIFS; i++)
				mfc.ttls[i] = 0;

			LIST_FOREACH(iface, &rdeconf->iface_list, entry) {
				if (mfc.ifindex != iface->ifindex)
					mfc.ttls[iface->ifindex] = 1;
			}

			mfc_update(&mfc);
#endif
			break;
		case IMSG_MFC_DEL:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(mfc))
				fatalx("invalid size of OE request");
			memcpy(&mfc, imsg.data, sizeof(mfc));
#if 1
			mfc_delete(&mfc);
#endif
			break;
		default:
			log_debug("rde_dispatch_msg: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}
