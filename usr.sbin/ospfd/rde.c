/*	$OpenBSD: rde.c,v 1.55 2007/01/29 13:04:13 claudio Exp $ */

/*
 * Copyright (c) 2004, 2005 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2004 Esben Norby <norby@openbsd.org>
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

#include "ospf.h"
#include "ospfd.h"
#include "ospfe.h"
#include "log.h"
#include "rde.h"

void		 rde_sig_handler(int sig, short, void *);
void		 rde_shutdown(void);
void		 rde_dispatch_imsg(int, short, void *);
void		 rde_dispatch_parent(int, short, void *);

void		 rde_send_summary(pid_t);
void		 rde_send_summary_area(struct area *, pid_t);
void		 rde_nbr_init(u_int32_t);
void		 rde_nbr_free(void);
struct rde_nbr	*rde_nbr_find(u_int32_t);
struct rde_nbr	*rde_nbr_new(u_int32_t, struct rde_nbr *);
void		 rde_nbr_del(struct rde_nbr *);

void		 rde_req_list_add(struct rde_nbr *, struct lsa_hdr *);
int		 rde_req_list_exists(struct rde_nbr *, struct lsa_hdr *);
void		 rde_req_list_del(struct rde_nbr *, struct lsa_hdr *);
void		 rde_req_list_free(struct rde_nbr *);

struct lsa	*rde_asext_get(struct rroute *);
struct lsa	*rde_asext_put(struct rroute *);

struct lsa	*orig_asext_lsa(struct rroute *, u_int16_t);
struct lsa	*orig_sum_lsa(struct rt_node *, u_int8_t);

struct ospfd_conf	*rdeconf = NULL;
struct imsgbuf		*ibuf_ospfe;
struct imsgbuf		*ibuf_main;
struct rde_nbr		*nbrself;
struct lsa_tree		 asext_tree;

/* ARGSUSED */
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
rde(struct ospfd_conf *xconf, int pipe_parent2rde[2], int pipe_ospfe2rde[2],
    int pipe_parent2ospfe[2])
{
	struct event		 ev_sigint, ev_sigterm;
	struct timeval		 now;
	struct area		*area;
	struct iface		*iface;
	struct passwd		*pw;
	struct redistribute	*r;
	pid_t			 pid;

	switch (pid = fork()) {
	case -1:
		fatal("cannot fork");
		/* NOTREACHED */
	case 0:
		break;
	default:
		return (pid);
	}

	rdeconf = xconf;

	if ((pw = getpwnam(OSPFD_USER)) == NULL)
		fatal("getpwnam");

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	setproctitle("route decision engine");
	ospfd_process = PROC_RDE_ENGINE;

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("can't drop privileges");

	event_init();
	rde_nbr_init(NBR_HASHSIZE);
	lsa_init(&asext_tree);

	/* setup signal handler */
	signal_set(&ev_sigint, SIGINT, rde_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, rde_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);

	/* setup pipes */
	close(pipe_ospfe2rde[0]);
	close(pipe_parent2rde[0]);
	close(pipe_parent2ospfe[0]);
	close(pipe_parent2ospfe[1]);

	if ((ibuf_ospfe = malloc(sizeof(struct imsgbuf))) == NULL ||
	    (ibuf_main = malloc(sizeof(struct imsgbuf))) == NULL)
		fatal(NULL);
	imsg_init(ibuf_ospfe, pipe_ospfe2rde[1], rde_dispatch_imsg);
	imsg_init(ibuf_main, pipe_parent2rde[1], rde_dispatch_parent);

	/* setup event handler */
	ibuf_ospfe->events = EV_READ;
	event_set(&ibuf_ospfe->ev, ibuf_ospfe->fd, ibuf_ospfe->events,
	    ibuf_ospfe->handler, ibuf_ospfe);
	event_add(&ibuf_ospfe->ev, NULL);

	ibuf_main->events = EV_READ;
	event_set(&ibuf_main->ev, ibuf_main->fd, ibuf_main->events,
	    ibuf_main->handler, ibuf_main);
	event_add(&ibuf_main->ev, NULL);

	evtimer_set(&rdeconf->ev, spf_timer, rdeconf);
	cand_list_init();
	rt_init();

	/* remove unneded stuff from config */
	LIST_FOREACH(area, &rdeconf->area_list, entry)
		LIST_FOREACH(iface, &area->iface_list, entry)
			md_list_clr(&iface->auth_md_list);
	
	while ((r = SIMPLEQ_FIRST(&rdeconf->redist_list)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&rdeconf->redist_list, entry);
		free(r);
	}

	gettimeofday(&now, NULL);
	rdeconf->uptime = now.tv_sec;

	event_dispatch();

	rde_shutdown();
	/* NOTREACHED */

	return (0);
}

void
rde_shutdown(void)
{
	struct area	*a;

	stop_spf_timer(rdeconf);
	cand_list_clr();
	rt_clear();

	while ((a = LIST_FIRST(&rdeconf->area_list)) != NULL) {
		LIST_REMOVE(a, entry);
		area_del(a);
	}
	rde_nbr_free();

	msgbuf_clear(&ibuf_ospfe->w);
	free(ibuf_ospfe);
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
rde_imsg_compose_ospfe(int type, u_int32_t peerid, pid_t pid, void *data,
    u_int16_t datalen)
{
	return (imsg_compose(ibuf_ospfe, type, peerid, pid, data, datalen));
}

/* ARGSUSED */
void
rde_dispatch_imsg(int fd, short event, void *bula)
{
	struct imsgbuf		*ibuf = bula;
	struct imsg		 imsg;
	struct in_addr		 aid;
	struct ls_req_hdr	 req_hdr;
	struct lsa_hdr		 lsa_hdr, *db_hdr;
	struct rde_nbr		 rn, *nbr;
	struct timespec		 tp;
	struct lsa		*lsa;
	struct area		*area;
	struct vertex		*v;
	char			*buf;
	ssize_t			 n;
	time_t			 now;
	int			 r, state, self;
	u_int16_t		 l;

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

	clock_gettime(CLOCK_MONOTONIC, &tp);
	now = tp.tv_sec;

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("rde_dispatch_imsg: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_NEIGHBOR_UP:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(rn))
				fatalx("invalid size of OE request");
			memcpy(&rn, imsg.data, sizeof(rn));

			if (rde_nbr_find(imsg.hdr.peerid))
				fatalx("rde_dispatch_imsg: "
				    "neighbor already exists");
			rde_nbr_new(imsg.hdr.peerid, &rn);
			break;
		case IMSG_NEIGHBOR_DOWN:
			rde_nbr_del(rde_nbr_find(imsg.hdr.peerid));
			break;
		case IMSG_NEIGHBOR_CHANGE:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(state))
				fatalx("invalid size of OE request");
			memcpy(&state, imsg.data, sizeof(state));

			nbr = rde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL)
				break;

			if (state != nbr->state && (nbr->state & NBR_STA_FULL ||
			    state & NBR_STA_FULL))
				area_track(nbr->area, state);

			nbr->state = state;
			if (nbr->state & NBR_STA_FULL)
				rde_req_list_free(nbr);
			break;
		case IMSG_DB_SNAPSHOT:
			nbr = rde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL)
				break;

			lsa_snap(nbr->area, imsg.hdr.peerid);

			imsg_compose(ibuf_ospfe, IMSG_DB_END, imsg.hdr.peerid,
			    0, NULL, 0);
			break;
		case IMSG_DD:
			nbr = rde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL)
				break;

			buf = imsg.data;
			for (l = imsg.hdr.len - IMSG_HEADER_SIZE;
			    l >= sizeof(lsa_hdr); l -= sizeof(lsa_hdr)) {
				memcpy(&lsa_hdr, buf, sizeof(lsa_hdr));
				buf += sizeof(lsa_hdr);

				v = lsa_find(nbr->area, lsa_hdr.type,
				    lsa_hdr.ls_id, lsa_hdr.adv_rtr);
				if (v == NULL)
					db_hdr = NULL;
				else
					db_hdr = &v->lsa->hdr;

				if (lsa_newer(&lsa_hdr, db_hdr) > 0) {
					/*
					 * only request LSAs that are
					 * newer or missing
					 */
					rde_req_list_add(nbr, &lsa_hdr);
					imsg_compose(ibuf_ospfe, IMSG_DD,
					    imsg.hdr.peerid, 0, &lsa_hdr,
					    sizeof(lsa_hdr));
				}
			}
			if (l != 0)
				log_warnx("rde_dispatch_imsg: peerid %lu, "
				    "trailing garbage in Database Description "
				    "packet", imsg.hdr.peerid);

			imsg_compose(ibuf_ospfe, IMSG_DD_END, imsg.hdr.peerid,
			    0, NULL, 0);
			break;
		case IMSG_LS_REQ:
			nbr = rde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL)
				break;

			buf = imsg.data;
			for (l = imsg.hdr.len - IMSG_HEADER_SIZE;
			    l >= sizeof(req_hdr); l -= sizeof(req_hdr)) {
				memcpy(&req_hdr, buf, sizeof(req_hdr));
				buf += sizeof(req_hdr);

				if ((v = lsa_find(nbr->area,
				    ntohl(req_hdr.type), req_hdr.ls_id,
				    req_hdr.adv_rtr)) == NULL) {
					imsg_compose(ibuf_ospfe, IMSG_LS_BADREQ,
					    imsg.hdr.peerid, 0, NULL, 0);
					continue;
				}
				imsg_compose(ibuf_ospfe, IMSG_LS_UPD,
				    imsg.hdr.peerid, 0, v->lsa,
				    ntohs(v->lsa->hdr.len));
			}
			if (l != 0)
				log_warnx("rde_dispatch_imsg: peerid %lu, "
				    "trailing garbage in LS Request "
				    "packet", imsg.hdr.peerid);
			break;
		case IMSG_LS_UPD:
			nbr = rde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL)
				break;

			lsa = malloc(imsg.hdr.len - IMSG_HEADER_SIZE);
			if (lsa == NULL)
				fatal(NULL);
			memcpy(lsa, imsg.data, imsg.hdr.len - IMSG_HEADER_SIZE);

			if (!lsa_check(nbr, lsa,
			    imsg.hdr.len - IMSG_HEADER_SIZE)) {
				free(lsa);
				break;
			}

			v = lsa_find(nbr->area, lsa->hdr.type, lsa->hdr.ls_id,
				    lsa->hdr.adv_rtr);
			if (v == NULL)
				db_hdr = NULL;
			else
				db_hdr = &v->lsa->hdr;

			if (nbr->self) {
				lsa_merge(nbr, lsa, v);
				/* lsa_merge frees the right lsa */
				break;
			}

			r = lsa_newer(&lsa->hdr, db_hdr);
			if (r > 0) {
				/* new LSA newer than DB */
				if (v && v->flooded &&
				    v->changed + MIN_LS_ARRIVAL >= now) {
					free(lsa);
					break;
				}

				rde_req_list_del(nbr, &lsa->hdr);

				if (!(self = lsa_self(nbr, lsa, v)))
					if (lsa_add(nbr, lsa))
						/* delayed lsa */
						break;

				/* flood and perhaps ack LSA */
				imsg_compose(ibuf_ospfe, IMSG_LS_FLOOD,
				    imsg.hdr.peerid, 0, lsa,
				    ntohs(lsa->hdr.len));

				/* reflood self originated LSA */
				if (self && v)
					imsg_compose(ibuf_ospfe, IMSG_LS_FLOOD,
					    v->peerid, 0, v->lsa,
					    ntohs(v->lsa->hdr.len));
				/* lsa not added so free it */
				if (self)
					free(lsa);
			} else if (r < 0) {
				/* lsa no longer needed */
				free(lsa);

				/*
				 * point 6 of "The Flooding Procedure"
				 * We are violating the RFC here because
				 * it does not make sense to reset a session
				 * because an equal LSA is already in the table.
				 * Only if the LSA sent is older than the one
				 * in the table we should reset the session.
				 */
				if (rde_req_list_exists(nbr, &lsa->hdr)) {
					imsg_compose(ibuf_ospfe, IMSG_LS_BADREQ,
					    imsg.hdr.peerid, 0, NULL, 0);
					break;
				}

				/* new LSA older than DB */
				if (ntohl(db_hdr->seq_num) == MAX_SEQ_NUM &&
				    ntohs(db_hdr->age) == MAX_AGE)
					/* seq-num wrap */
					break;

				if (v->changed + MIN_LS_ARRIVAL >= now)
					break;

				/* directly send current LSA, no ack */
				imsg_compose(ibuf_ospfe, IMSG_LS_UPD,
				    imsg.hdr.peerid, 0, v->lsa,
				    ntohs(v->lsa->hdr.len));
			} else {
				/* LSA equal send direct ack */
				imsg_compose(ibuf_ospfe, IMSG_LS_ACK,
				    imsg.hdr.peerid, 0, &lsa->hdr,
				    sizeof(lsa->hdr));
				free(lsa);
			}
			break;
		case IMSG_LS_MAXAGE:
			nbr = rde_nbr_find(imsg.hdr.peerid);
			if (nbr == NULL)
				break;

			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct lsa_hdr))
				fatalx("invalid size of OE request");
			memcpy(&lsa_hdr, imsg.data, sizeof(lsa_hdr));

			aid.s_addr = lsa_hdr.ls_id;

			if (rde_nbr_loading(nbr->area))
				break;

			v = lsa_find(nbr->area, lsa_hdr.type, lsa_hdr.ls_id,
				    lsa_hdr.adv_rtr);
			if (v == NULL)
				db_hdr = NULL;
			else
				db_hdr = &v->lsa->hdr;

			/*
			 * only delete LSA if the one in the db is not newer
			 */
			if (lsa_newer(db_hdr, &lsa_hdr) <= 0)
				lsa_del(nbr, &lsa_hdr);
			break;
		case IMSG_CTL_SHOW_DATABASE:
		case IMSG_CTL_SHOW_DB_EXT:
		case IMSG_CTL_SHOW_DB_NET:
		case IMSG_CTL_SHOW_DB_RTR:
		case IMSG_CTL_SHOW_DB_SELF:
		case IMSG_CTL_SHOW_DB_SUM:
		case IMSG_CTL_SHOW_DB_ASBR:
			if (imsg.hdr.len != IMSG_HEADER_SIZE &&
			    imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(aid)) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			if (imsg.hdr.len == IMSG_HEADER_SIZE) {
				LIST_FOREACH(area, &rdeconf->area_list, entry) {
					imsg_compose(ibuf_ospfe, IMSG_CTL_AREA,
					    0, imsg.hdr.pid, area,
					    sizeof(*area));
					lsa_dump(&area->lsa_tree, imsg.hdr.type,
					    imsg.hdr.pid);
				}
				lsa_dump(&asext_tree, imsg.hdr.type,
				    imsg.hdr.pid);
			} else {
				memcpy(&aid, imsg.data, sizeof(aid));
				if ((area = area_find(rdeconf, aid)) != NULL) {
					imsg_compose(ibuf_ospfe, IMSG_CTL_AREA,
					    0, imsg.hdr.pid, area,
					    sizeof(*area));
					lsa_dump(&area->lsa_tree, imsg.hdr.type,
					    imsg.hdr.pid);
					if (!area->stub)
						lsa_dump(&asext_tree,
						    imsg.hdr.type,
						    imsg.hdr.pid);
				}
			}
			imsg_compose(ibuf_ospfe, IMSG_CTL_END, 0, imsg.hdr.pid,
			    NULL, 0);
			break;
		case IMSG_CTL_SHOW_RIB:
			LIST_FOREACH(area, &rdeconf->area_list, entry) {
				imsg_compose(ibuf_ospfe, IMSG_CTL_AREA,
				    0, imsg.hdr.pid, area, sizeof(*area));

				rt_dump(area->id, imsg.hdr.pid, RIB_RTR);
				rt_dump(area->id, imsg.hdr.pid, RIB_NET);
			}
			aid.s_addr = 0;
			rt_dump(aid, imsg.hdr.pid, RIB_EXT);

			imsg_compose(ibuf_ospfe, IMSG_CTL_END, 0, imsg.hdr.pid,
			    NULL, 0);
			break;
		case IMSG_CTL_SHOW_SUM:
			rde_send_summary(imsg.hdr.pid);
			LIST_FOREACH(area, &rdeconf->area_list, entry)
				rde_send_summary_area(area, imsg.hdr.pid);
			imsg_compose(ibuf_ospfe, IMSG_CTL_END, 0, imsg.hdr.pid,
			    NULL, 0);
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

/* ARGSUSED */
void
rde_dispatch_parent(int fd, short event, void *bula)
{
	struct imsg		 imsg;
	struct kroute		 kr;
	struct rroute		 rr;
	struct imsgbuf		*ibuf = bula;
	struct lsa		*lsa;
	struct vertex		*v;
	struct rt_node		*rn;
	ssize_t			 n;

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
			fatal("rde_dispatch_parent: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_NETWORK_ADD:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(rr)) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			memcpy(&rr, imsg.data, sizeof(rr));

			if ((lsa = rde_asext_get(&rr)) != NULL) {
				v = lsa_find(NULL, lsa->hdr.type,
				    lsa->hdr.ls_id, lsa->hdr.adv_rtr);

				lsa_merge(nbrself, lsa, v);
			}
			break;
		case IMSG_NETWORK_DEL:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(rr)) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			memcpy(&rr, imsg.data, sizeof(rr));

			if ((lsa = rde_asext_put(&rr)) != NULL) {
				v = lsa_find(NULL, lsa->hdr.type,
				    lsa->hdr.ls_id, lsa->hdr.adv_rtr);

				/*
				 * if v == NULL no LSA is in the table and
				 * nothing has to be done.
				 */
				if (v)
					lsa_merge(nbrself, lsa, v);
			}
			break;
		case IMSG_KROUTE_GET:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(kr)) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			memcpy(&kr, imsg.data, sizeof(kr));

			if ((rn = rt_find(kr.prefix.s_addr, kr.prefixlen,
			    DT_NET)) != NULL)
				rde_send_change_kroute(rn);
			else
				/* should not happen */
				imsg_compose(ibuf_main, IMSG_KROUTE_DELETE, 0,
				    0, &kr, sizeof(kr));

			break;
		default:
			log_debug("rde_dispatch_parent: unexpected imsg %d",
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	imsg_event_add(ibuf);
}

u_int32_t
rde_router_id(void)
{
	return (rdeconf->rtr_id.s_addr);
}

void
rde_send_change_kroute(struct rt_node *r)
{
	struct kroute	 kr;

	bzero(&kr, sizeof(kr));
	kr.prefix.s_addr = r->prefix.s_addr;
	kr.nexthop.s_addr = r->nexthop.s_addr;
	kr.prefixlen = r->prefixlen;

	imsg_compose(ibuf_main, IMSG_KROUTE_CHANGE, 0, 0, &kr, sizeof(kr));
}

void
rde_send_delete_kroute(struct rt_node *r)
{
	struct kroute	 kr;

	bzero(&kr, sizeof(kr));
	kr.prefix.s_addr = r->prefix.s_addr;
	kr.nexthop.s_addr = r->nexthop.s_addr;
	kr.prefixlen = r->prefixlen;

	imsg_compose(ibuf_main, IMSG_KROUTE_DELETE, 0, 0, &kr, sizeof(kr));
}

void
rde_send_summary(pid_t pid)
{
	static struct ctl_sum	 sumctl;
	struct timeval		 now;
	struct area		*area;
	struct vertex		*v;

	bzero(&sumctl, sizeof(struct ctl_sum));

	sumctl.rtr_id.s_addr = rde_router_id();
	sumctl.spf_delay = rdeconf->spf_delay;
	sumctl.spf_hold_time = rdeconf->spf_hold_time;

	LIST_FOREACH(area, &rdeconf->area_list, entry)
		sumctl.num_area++;

	RB_FOREACH(v, lsa_tree, &asext_tree)
		sumctl.num_ext_lsa++;

	gettimeofday(&now, NULL);
	if (rdeconf->uptime < now.tv_sec)
		sumctl.uptime = now.tv_sec - rdeconf->uptime;
	else
		sumctl.uptime = 0;

	sumctl.rfc1583compat = rdeconf->rfc1583compat;

	rde_imsg_compose_ospfe(IMSG_CTL_SHOW_SUM, 0, pid, &sumctl,
	    sizeof(sumctl));
}

void
rde_send_summary_area(struct area *area, pid_t pid)
{
	static struct ctl_sum_area	 sumareactl;
	struct iface			*iface;
	struct rde_nbr			*nbr;
	struct lsa_tree			*tree = &area->lsa_tree;
	struct vertex			*v;

	bzero(&sumareactl, sizeof(struct ctl_sum_area));

	sumareactl.area.s_addr = area->id.s_addr;
	sumareactl.num_spf_calc = area->num_spf_calc;

	LIST_FOREACH(iface, &area->iface_list, entry)
		sumareactl.num_iface++;

	LIST_FOREACH(nbr, &area->nbr_list, entry)
		if (nbr->state == NBR_STA_FULL && !nbr->self)
			sumareactl.num_adj_nbr++;

	RB_FOREACH(v, lsa_tree, tree)
		sumareactl.num_lsa++;

	rde_imsg_compose_ospfe(IMSG_CTL_SHOW_SUM_AREA, 0, pid, &sumareactl,
	    sizeof(sumareactl));
}

LIST_HEAD(rde_nbr_head, rde_nbr);

struct nbr_table {
	struct rde_nbr_head	*hashtbl;
	u_int32_t		 hashmask;
} rdenbrtable;

#define RDE_NBR_HASH(x)		\
	&rdenbrtable.hashtbl[(x) & rdenbrtable.hashmask]

void
rde_nbr_init(u_int32_t hashsize)
{
	struct rde_nbr_head	*head;
	u_int32_t		 hs, i;

	for (hs = 1; hs < hashsize; hs <<= 1)
		;
	rdenbrtable.hashtbl = calloc(hs, sizeof(struct rde_nbr_head));
	if (rdenbrtable.hashtbl == NULL)
		fatal("rde_nbr_init");

	for (i = 0; i < hs; i++)
		LIST_INIT(&rdenbrtable.hashtbl[i]);

	rdenbrtable.hashmask = hs - 1;

	if ((nbrself = calloc(1, sizeof(*nbrself))) == NULL)
		fatal("rde_nbr_init");

	nbrself->id.s_addr = rde_router_id();
	nbrself->peerid = NBR_IDSELF;
	nbrself->state = NBR_STA_DOWN;
	nbrself->self = 1;
	head = RDE_NBR_HASH(NBR_IDSELF);
	LIST_INSERT_HEAD(head, nbrself, hash);
}

void
rde_nbr_free(void)
{
	free(nbrself);
	free(rdenbrtable.hashtbl);
}

struct rde_nbr *
rde_nbr_find(u_int32_t peerid)
{
	struct rde_nbr_head	*head;
	struct rde_nbr		*nbr;

	head = RDE_NBR_HASH(peerid);

	LIST_FOREACH(nbr, head, hash) {
		if (nbr->peerid == peerid)
			return (nbr);
	}

	return (NULL);
}

struct rde_nbr *
rde_nbr_new(u_int32_t peerid, struct rde_nbr *new)
{
	struct rde_nbr_head	*head;
	struct rde_nbr		*nbr;
	struct area		*area;

	if (rde_nbr_find(peerid))
		return (NULL);
	if ((area = area_find(rdeconf, new->area_id)) == NULL)
		fatalx("rde_nbr_new: unknown area");

	if ((nbr = calloc(1, sizeof(*nbr))) == NULL)
		fatal("rde_nbr_new");

	memcpy(nbr, new, sizeof(*nbr));
	nbr->peerid = peerid;
	nbr->area = area;

	TAILQ_INIT(&nbr->req_list);

	head = RDE_NBR_HASH(peerid);
	LIST_INSERT_HEAD(head, nbr, hash);
	LIST_INSERT_HEAD(&area->nbr_list, nbr, entry);

	return (nbr);
}

void
rde_nbr_del(struct rde_nbr *nbr)
{
	if (nbr == NULL)
		return;

	rde_req_list_free(nbr);

	LIST_REMOVE(nbr, entry);
	LIST_REMOVE(nbr, hash);

	free(nbr);
}

int
rde_nbr_loading(struct area *area)
{
	struct rde_nbr		*nbr;

	LIST_FOREACH(nbr, &area->nbr_list, entry) {
		if (nbr->self)
			continue;
		if (nbr->state & NBR_STA_XCHNG ||
		    nbr->state & NBR_STA_LOAD)
			return (1);
	}
	return (0);
}

struct rde_nbr *
rde_nbr_self(struct area *area)
{
	struct rde_nbr		*nbr;

	LIST_FOREACH(nbr, &area->nbr_list, entry)
		if (nbr->self)
			return (nbr);

	/* this may not happen */
	fatalx("rde_nbr_self: area without self");
	return (NULL);
}

/*
 * LSA req list
 */
void
rde_req_list_add(struct rde_nbr *nbr, struct lsa_hdr *lsa)
{
	struct rde_req_entry	*le;

	if ((le = calloc(1, sizeof(*le))) == NULL)
		fatal("rde_req_list_add");

	TAILQ_INSERT_TAIL(&nbr->req_list, le, entry);
	le->type = lsa->type;
	le->ls_id = lsa->ls_id;
	le->adv_rtr = lsa->adv_rtr;
}

int
rde_req_list_exists(struct rde_nbr *nbr, struct lsa_hdr *lsa_hdr)
{
	struct rde_req_entry	*le;

	TAILQ_FOREACH(le, &nbr->req_list, entry) {
		if ((lsa_hdr->type == le->type) &&
		    (lsa_hdr->ls_id == le->ls_id) &&
		    (lsa_hdr->adv_rtr == le->adv_rtr))
			return (1);
	}
	return (0);
}

void
rde_req_list_del(struct rde_nbr *nbr, struct lsa_hdr *lsa_hdr)
{
	struct rde_req_entry	*le;

	TAILQ_FOREACH(le, &nbr->req_list, entry) {
		if ((lsa_hdr->type == le->type) &&
		    (lsa_hdr->ls_id == le->ls_id) &&
		    (lsa_hdr->adv_rtr == le->adv_rtr)) {
			TAILQ_REMOVE(&nbr->req_list, le, entry);
			free(le);
			return;
		}
	}
}

void
rde_req_list_free(struct rde_nbr *nbr)
{
	struct rde_req_entry	*le;

	while ((le = TAILQ_FIRST(&nbr->req_list)) != NULL) {
		TAILQ_REMOVE(&nbr->req_list, le, entry);
		free(le);
	}
}

/*
 * as-external LSA handling
 */
struct lsa *
rde_asext_get(struct rroute *rr)
{
	struct area	*area;
	struct iface	*iface;

	LIST_FOREACH(area, &rdeconf->area_list, entry)
		LIST_FOREACH(iface, &area->iface_list, entry) {
			if ((iface->addr.s_addr & iface->mask.s_addr) ==
			    rr->kr.prefix.s_addr && iface->mask.s_addr ==
			    prefixlen2mask(rr->kr.prefixlen)) {
				/* already announced as (stub) net LSA */
				log_debug("rde_asext_get: %s/%d is net LSA",
				    inet_ntoa(rr->kr.prefix), rr->kr.prefixlen);
				return (NULL);
			}
		}

	/* update of seqnum is done by lsa_merge */
	return (orig_asext_lsa(rr, DEFAULT_AGE));
}

struct lsa *
rde_asext_put(struct rroute *rr)
{
	/* 
	 * just try to remove the LSA. If the prefix is announced as
	 * stub net LSA lsa_find() will fail later and nothing will happen.
	 */

	/* remove by reflooding with MAX_AGE */
	return (orig_asext_lsa(rr, MAX_AGE));
}

/*
 * summary LSA stuff
 */
void
rde_summary_update(struct rt_node *rte, struct area *area)
{
	struct vertex	*v = NULL;
	struct lsa	*lsa;
	u_int8_t	 type = 0;

	/* first check if we actually need to announce this route */
	if (!(rte->d_type == DT_NET || rte->flags & OSPF_RTR_E))
		return;
	/* never create summaries for as-ext LSA */
	if (rte->p_type == PT_TYPE1_EXT || rte->p_type == PT_TYPE2_EXT)
		return;
	/* no need for summary LSA in the originating area */
	if (rte->area.s_addr == area->id.s_addr)
		return;
	/* TODO nexthop check, nexthop part of area -> no summary */
	if (rte->cost >= LS_INFINITY)
		return;
	/* TODO AS border router specific checks */
	/* TODO inter-area network route stuff */
	/* TODO intra-area stuff -- condense LSA ??? */

	/* update lsa but only if it was changed */
	if (rte->d_type == DT_NET) {
		type = LSA_TYPE_SUM_NETWORK;
		v = lsa_find(area, type, rte->prefix.s_addr, rde_router_id());
	} else if (rte->d_type == DT_RTR) {
		type = LSA_TYPE_SUM_ROUTER;
		v = lsa_find(area, type, rte->adv_rtr.s_addr, rde_router_id());
	} else
		fatalx("orig_sum_lsa: unknown route type");

	lsa = orig_sum_lsa(rte, type);
	lsa_merge(rde_nbr_self(area), lsa, v);

	if (v == NULL) {
		if (rte->d_type == DT_NET)
			v = lsa_find(area, type, rte->prefix.s_addr,
			    rde_router_id());
		else
			v = lsa_find(area, type, rte->adv_rtr.s_addr,
			    rde_router_id());
	}
	/* suppressed/deleted routes are not found in the second lsa_find */ 
	if (v)
		v->cost = rte->cost;
}


/*
 * functions for self-originated LSA
 */
struct lsa *
orig_asext_lsa(struct rroute *rr, u_int16_t age)
{
	struct lsa	*lsa;
	u_int16_t	 len;

	len = sizeof(struct lsa_hdr) + sizeof(struct lsa_asext);
	if ((lsa = calloc(1, len)) == NULL)
		fatal("orig_asext_lsa");

	log_debug("orig_asext_lsa: %s/%d age %d",
	    inet_ntoa(rr->kr.prefix), rr->kr.prefixlen, age);

	/* LSA header */
	lsa->hdr.age = htons(age);
	lsa->hdr.opts = rdeconf->options;	/* XXX not updated */
	lsa->hdr.type = LSA_TYPE_EXTERNAL;
	lsa->hdr.adv_rtr = rdeconf->rtr_id.s_addr;
	lsa->hdr.seq_num = htonl(INIT_SEQ_NUM);
	lsa->hdr.len = htons(len);

	/* prefix and mask */
	/*
	 * TODO ls_id must be unique, for overlapping routes this may
	 * not be true. In this case a hack needs to be done to
	 * make the ls_id unique.
	 */
	lsa->hdr.ls_id = rr->kr.prefix.s_addr;
	lsa->data.asext.mask = prefixlen2mask(rr->kr.prefixlen);

	/*
	 * nexthop -- on connected routes we are the nexthop,
	 * on all other cases we announce the true nexthop.
	 * XXX this is wrong as the true nexthop may be outside
	 * of the ospf cloud and so unreachable. For now we force
	 * all traffic to be directed to us.
	 */
	lsa->data.asext.fw_addr = 0;

	lsa->data.asext.metric = htonl(rr->metric);
	lsa->data.asext.ext_tag = 0;

	lsa->hdr.ls_chksum = 0;
	lsa->hdr.ls_chksum =
	    htons(iso_cksum(lsa, len, LS_CKSUM_OFFSET));

	return (lsa);
}

struct lsa *
orig_sum_lsa(struct rt_node *rte, u_int8_t type)
{
	struct lsa	*lsa;
	u_int16_t	 len;

	len = sizeof(struct lsa_hdr) + sizeof(struct lsa_sum);
	if ((lsa = calloc(1, len)) == NULL)
		fatal("orig_sum_lsa");

	/* LSA header */
	lsa->hdr.age = htons(rte->invalid ? MAX_AGE : DEFAULT_AGE);
	lsa->hdr.opts = rdeconf->options;	/* XXX not updated */
	lsa->hdr.type = type;
	lsa->hdr.adv_rtr = rdeconf->rtr_id.s_addr;
	lsa->hdr.seq_num = htonl(INIT_SEQ_NUM);
	lsa->hdr.len = htons(len);

	/* prefix and mask */
	/*
	 * TODO ls_id must be unique, for overlapping routes this may
	 * not be true. In this case a hack needs to be done to
	 * make the ls_id unique.
	 */
	if (type == LSA_TYPE_SUM_NETWORK) {
		lsa->hdr.ls_id = rte->prefix.s_addr;
		lsa->data.sum.mask = prefixlen2mask(rte->prefixlen);
	} else {
		lsa->hdr.ls_id = rte->adv_rtr.s_addr;
		lsa->data.sum.mask = 0;	/* must be zero per RFC */
	}

	lsa->data.sum.metric = htonl(rte->cost & LSA_METRIC_MASK);

	lsa->hdr.ls_chksum = 0;
	lsa->hdr.ls_chksum =
	    htons(iso_cksum(lsa, len, LS_CKSUM_OFFSET));

	return (lsa);
}

