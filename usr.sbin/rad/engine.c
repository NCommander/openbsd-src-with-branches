/*	$OpenBSD: engine.c,v 1.2 2018/07/10 22:13:16 florian Exp $	*/

/*
 * Copyright (c) 2018 Florian Obser <florian@openbsd.org>
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
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/icmp6.h>

#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>

#include "log.h"
#include "rad.h"
#include "engine.h"

__dead void	 engine_shutdown(void);
void		 engine_sig_handler(int sig, short, void *);
void		 engine_dispatch_frontend(int, short, void *);
void		 engine_dispatch_main(int, short, void *);
void		 parse_ra_rs(struct imsg_ra_rs *);
void		 parse_ra(struct imsg_ra_rs *);
void		 parse_rs(struct imsg_ra_rs *);

struct rad_conf	*engine_conf;
struct imsgev		*iev_frontend;
struct imsgev		*iev_main;
struct sockaddr_in6	 all_nodes;

void
engine_sig_handler(int sig, short event, void *arg)
{
	/*
	 * Normal signal handler rules don't apply because libevent
	 * decouples for us.
	 */

	switch (sig) {
	case SIGINT:
	case SIGTERM:
		engine_shutdown();
	default:
		fatalx("unexpected signal");
	}
}

void
engine(int debug, int verbose)
{
	struct event		 ev_sigint, ev_sigterm;
	struct passwd		*pw;

	engine_conf = config_new_empty();

	log_init(debug, LOG_DAEMON);
	log_setverbose(verbose);

	if ((pw = getpwnam(RAD_USER)) == NULL)
		fatal("getpwnam");

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	rad_process = PROC_ENGINE;
	setproctitle("%s", log_procnames[rad_process]);
	log_procinit(log_procnames[rad_process]);

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("can't drop privileges");

	if (pledge("stdio recvfd", NULL) == -1)
		fatal("pledge");

	event_init();

	/* Setup signal handler(s). */
	signal_set(&ev_sigint, SIGINT, engine_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, engine_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	/* Setup pipe and event handler to the main process. */
	if ((iev_main = malloc(sizeof(struct imsgev))) == NULL)
		fatal(NULL);

	imsg_init(&iev_main->ibuf, 3);
	iev_main->handler = engine_dispatch_main;

	/* Setup event handlers. */
	iev_main->events = EV_READ;
	event_set(&iev_main->ev, iev_main->ibuf.fd, iev_main->events,
	    iev_main->handler, iev_main);
	event_add(&iev_main->ev, NULL);

	all_nodes.sin6_len = sizeof(all_nodes);
	all_nodes.sin6_family = AF_INET6;
	if (inet_pton(AF_INET6, "ff02::1", &all_nodes.sin6_addr) != 1)
		fatal("inet_pton");

	event_dispatch();

	engine_shutdown();
}

__dead void
engine_shutdown(void)
{
	/* Close pipes. */
	msgbuf_clear(&iev_frontend->ibuf.w);
	close(iev_frontend->ibuf.fd);
	msgbuf_clear(&iev_main->ibuf.w);
	close(iev_main->ibuf.fd);

	config_clear(engine_conf);

	free(iev_frontend);
	free(iev_main);

	log_info("engine exiting");
	exit(0);
}

int
engine_imsg_compose_frontend(int type, pid_t pid, void *data, uint16_t datalen)
{
	return (imsg_compose_event(iev_frontend, type, 0, pid, -1,
	    data, datalen));
}

void
engine_dispatch_frontend(int fd, short event, void *bula)
{
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf;
	struct imsg		 imsg;
	struct imsg_ra_rs	 ra_rs;
	ssize_t			 n;
	int			 shut = 0, verbose;

	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("imsg_read error");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("msgbuf_write");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("%s: imsg_get error", __func__);
		if (n == 0)	/* No more messages. */
			break;

		switch (imsg.hdr.type) {
		case IMSG_RA_RS:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(ra_rs))
				fatal("%s: IMSG_RA_RS wrong length: %d",
				    __func__, imsg.hdr.len);
			memcpy(&ra_rs, imsg.data, sizeof(ra_rs));
			parse_ra_rs(&ra_rs);
			break;
		case IMSG_CTL_LOG_VERBOSE:
			/* Already checked by frontend. */
			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_setverbose(verbose);
			break;
		default:
			log_debug("%s: unexpected imsg %d", __func__,
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	if (!shut)
		imsg_event_add(iev);
	else {
		/* This pipe is dead. Remove its event handler. */
		event_del(&iev->ev);
		event_loopexit(NULL);
	}
}

void
engine_dispatch_main(int fd, short event, void *bula)
{
	static struct rad_conf		*nconf;
	static struct ra_iface_conf	*ra_iface_conf;
	struct imsg			 imsg;
	struct imsgev			*iev = bula;
	struct imsgbuf			*ibuf;
	struct ra_prefix_conf		*ra_prefix_conf;
	ssize_t				 n;
	int				 shut = 0;

	ibuf = &iev->ibuf;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("imsg_read error");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("msgbuf_write");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("%s: imsg_get error", __func__);
		if (n == 0)	/* No more messages. */
			break;

		switch (imsg.hdr.type) {
		case IMSG_SOCKET_IPC:
			/*
			 * Setup pipe and event handler to the frontend
			 * process.
			 */
			if (iev_frontend) {
				log_warnx("%s: received unexpected imsg fd "
				    "to engine", __func__);
				break;
			}
			if ((fd = imsg.fd) == -1) {
				log_warnx("%s: expected to receive imsg fd to "
				   "engine but didn't receive any", __func__);
				break;
			}

			iev_frontend = malloc(sizeof(struct imsgev));
			if (iev_frontend == NULL)
				fatal(NULL);

			imsg_init(&iev_frontend->ibuf, fd);
			iev_frontend->handler = engine_dispatch_frontend;
			iev_frontend->events = EV_READ;

			event_set(&iev_frontend->ev, iev_frontend->ibuf.fd,
			iev_frontend->events, iev_frontend->handler,
			    iev_frontend);
			event_add(&iev_frontend->ev, NULL);
			break;
		case IMSG_RECONF_CONF:
			if ((nconf = malloc(sizeof(struct rad_conf))) == NULL)
				fatal(NULL);
			memcpy(nconf, imsg.data, sizeof(struct rad_conf));
			SIMPLEQ_INIT(&nconf->ra_iface_list);
			break;
		case IMSG_RECONF_RA_IFACE:
			if ((ra_iface_conf = malloc(sizeof(struct
			    ra_iface_conf))) == NULL)
				fatal(NULL);
			memcpy(ra_iface_conf, imsg.data,
			    sizeof(struct ra_iface_conf));
			ra_iface_conf->autoprefix = NULL;
			SIMPLEQ_INIT(&ra_iface_conf->ra_prefix_list);
			SIMPLEQ_INSERT_TAIL(&nconf->ra_iface_list,
			    ra_iface_conf, entry);
			break;
		case IMSG_RECONF_RA_AUTOPREFIX:
			if ((ra_iface_conf->autoprefix = malloc(sizeof(struct
			    ra_prefix_conf))) == NULL)
				fatal(NULL);
			memcpy(ra_iface_conf->autoprefix, imsg.data,
			    sizeof(struct ra_prefix_conf));
			break;
		case IMSG_RECONF_RA_PREFIX:
			if ((ra_prefix_conf = malloc(sizeof(struct
			    ra_prefix_conf))) == NULL)
				fatal(NULL);
			memcpy(ra_prefix_conf, imsg.data, sizeof(struct
			    ra_prefix_conf));
			SIMPLEQ_INSERT_TAIL(&ra_iface_conf->ra_prefix_list,
			    ra_prefix_conf, entry);
			break;
		case IMSG_RECONF_END:
			merge_config(engine_conf, nconf);
			nconf = NULL;
			break;
		default:
			log_debug("%s: unexpected imsg %d", __func__,
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	if (!shut)
		imsg_event_add(iev);
	else {
		/* This pipe is dead. Remove its event handler. */
		event_del(&iev->ev);
		event_loopexit(NULL);
	}
}


void
parse_ra_rs(struct imsg_ra_rs *ra_rs)
{
	char			 ntopbuf[INET6_ADDRSTRLEN], ifnamebuf[IFNAMSIZ];
	struct icmp6_hdr	*hdr;

	hdr = (struct icmp6_hdr *) ra_rs->packet;

	switch (hdr->icmp6_type) {
	case ND_ROUTER_ADVERT:
		parse_ra(ra_rs);
		break;
	case ND_ROUTER_SOLICIT:
		parse_rs(ra_rs);
		break;
	default:
		log_warnx("unexpected icmp6_type: %d from %s on %s",
		    hdr->icmp6_type, inet_ntop(AF_INET6, &ra_rs->from.sin6_addr,
		    ntopbuf, INET6_ADDRSTRLEN), if_indextoname(ra_rs->if_index,
		    ifnamebuf));
		break;
	}
}

void
parse_ra(struct imsg_ra_rs *ra)
{
	char			 ntopbuf[INET6_ADDRSTRLEN], ifnamebuf[IFNAMSIZ];
	log_debug("got RA from %s on %s",
	    inet_ntop(AF_INET6, &ra->from.sin6_addr, ntopbuf,
	    INET6_ADDRSTRLEN), if_indextoname(ra->if_index,
	    ifnamebuf));
	/* XXX not yet */
}

void
parse_rs(struct imsg_ra_rs *rs)
{
	struct nd_router_solicit	*nd_rs;
	struct imsg_send_ra		 send_ra;
	ssize_t				 len;
	const char			*hbuf;
	char				 ifnamebuf[IFNAMSIZ];
	uint8_t				*p;

	hbuf = sin6_to_str(&rs->from);

	send_ra.if_index = rs->if_index;
	memcpy(&send_ra.to, &all_nodes, sizeof(send_ra.to));

	log_debug("got RS from %s on %s", hbuf, if_indextoname(rs->if_index,
	    ifnamebuf));

	len = rs->len;

	if (!IN6_IS_ADDR_LINKLOCAL(&rs->from.sin6_addr)) {
		log_warnx("RA from non link local address %s", hbuf);
		return;
	}

	if ((size_t)len < sizeof(struct nd_router_solicit)) {
		log_warnx("received too short message (%ld) from %s", len,
		    hbuf);
		return;
	}

	p = rs->packet;
	nd_rs = (struct nd_router_solicit *)p;
	len -= sizeof(struct nd_router_solicit);
	p += sizeof(struct nd_router_solicit);

	if (nd_rs->nd_rs_code != 0) {
		log_warnx("invalid ICMPv6 code (%d) from %s", nd_rs->nd_rs_code,
		    hbuf);
		return;
	}
	while ((size_t)len >= sizeof(struct nd_opt_hdr)) {
		struct nd_opt_hdr *nd_opt_hdr = (struct nd_opt_hdr *)p;

		len -= sizeof(struct nd_opt_hdr);
		p += sizeof(struct nd_opt_hdr);

		if (nd_opt_hdr->nd_opt_len * 8 - 2 > len) {
			log_warnx("invalid option len: %u > %ld",
			    nd_opt_hdr->nd_opt_len, len);
			return;
		}
		switch (nd_opt_hdr->nd_opt_type) {
		case ND_OPT_SOURCE_LINKADDR:
			log_debug("got RS with source linkaddr option");
			memcpy(&send_ra.to, &rs->from, sizeof(send_ra.to));
			break;
		default:
			log_debug("\t\tUNKNOWN: %d", nd_opt_hdr->nd_opt_type);
			break;
		}
		len -= nd_opt_hdr->nd_opt_len * 8 - 2;
		p += nd_opt_hdr->nd_opt_len * 8 - 2;
	}
	engine_imsg_compose_frontend(IMSG_SEND_RA, 0, &send_ra,
	    sizeof(send_ra));
}
