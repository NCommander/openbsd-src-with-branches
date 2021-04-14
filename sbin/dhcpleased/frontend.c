/*	$OpenBSD: frontend.c,v 1.9 2021/04/08 17:29:17 kn Exp $	*/

/*
 * Copyright (c) 2017, 2021 Florian Obser <florian@openbsd.org>
 * Copyright (c) 2005 Claudio Jeker <claudio@openbsd.org>
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
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/uio.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#include <arpa/inet.h>

#include <errno.h>
#include <event.h>
#include <ifaddrs.h>
#include <imsg.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bpf.h"
#include "log.h"
#include "dhcpleased.h"
#include "frontend.h"
#include "control.h"
#include "checksum.h"

#define	ROUTE_SOCKET_BUF_SIZE	16384
#define	BOOTP_MIN_LEN		300	/* fixed bootp packet adds up to 300 */

struct bpf_ev {
	struct event		 ev;
	uint8_t			 buf[BPFLEN];
};

struct iface {
	LIST_ENTRY(iface)	 entries;
	struct bpf_ev		 bpfev;
	struct imsg_ifinfo	 ifinfo;
	int			 send_discover;
	uint32_t		 xid;
	struct in_addr		 requested_ip;
	struct in_addr		 server_identifier;
	struct in_addr		 dhcp_server;
	int			 udpsock;
};

__dead void	 frontend_shutdown(void);
void		 frontend_sig_handler(int, short, void *);
void		 update_iface(struct if_msghdr *, struct sockaddr_dl *);
void		 frontend_startup(void);
void		 init_ifaces(void);
void		 route_receive(int, short, void *);
void		 handle_route_message(struct rt_msghdr *, struct sockaddr **);
void		 get_rtaddrs(int, struct sockaddr *, struct sockaddr **);
void		 bpf_receive(int, short, void *);
int		 get_flags(char *);
int		 get_xflags(char *);
struct iface	*get_iface_by_id(uint32_t);
void		 remove_iface(uint32_t);
void		 set_bpfsock(int, uint32_t);
ssize_t		 build_packet(uint8_t, uint32_t, struct ether_addr *, struct
		     in_addr *, struct in_addr *);
void		 send_discover(struct iface *);
void		 send_request(struct iface *);
void		 bpf_send_packet(struct iface *, uint8_t *, ssize_t);
void		 udp_send_packet(struct iface *, uint8_t *, ssize_t);

LIST_HEAD(, iface)		 interfaces;
static struct imsgev		*iev_main;
static struct imsgev		*iev_engine;
struct event			 ev_route;
int				 ioctlsock;

uint8_t				 dhcp_packet[1500];

void
frontend_sig_handler(int sig, short event, void *bula)
{
	/*
	 * Normal signal handler rules don't apply because libevent
	 * decouples for us.
	 */

	switch (sig) {
	case SIGINT:
	case SIGTERM:
		frontend_shutdown();
	default:
		fatalx("unexpected signal");
	}
}

void
frontend(int debug, int verbose)
{
	struct event		 ev_sigint, ev_sigterm;
	struct passwd		*pw;

	log_init(debug, LOG_DAEMON);
	log_setverbose(verbose);

	if ((pw = getpwnam(DHCPLEASED_USER)) == NULL)
		fatal("getpwnam");

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	setproctitle("%s", "frontend");
	log_procinit("frontend");

	if ((ioctlsock = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0)) == -1)
		fatal("socket");

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("can't drop privileges");

	if (pledge("stdio unix recvfd route", NULL) == -1)
		fatal("pledge");
	event_init();

	/* Setup signal handler. */
	signal_set(&ev_sigint, SIGINT, frontend_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, frontend_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	/* Setup pipe and event handler to the parent process. */
	if ((iev_main = malloc(sizeof(struct imsgev))) == NULL)
		fatal(NULL);
	imsg_init(&iev_main->ibuf, 3);
	iev_main->handler = frontend_dispatch_main;
	iev_main->events = EV_READ;
	event_set(&iev_main->ev, iev_main->ibuf.fd, iev_main->events,
	    iev_main->handler, iev_main);
	event_add(&iev_main->ev, NULL);

	LIST_INIT(&interfaces);
	event_dispatch();

	frontend_shutdown();
}

__dead void
frontend_shutdown(void)
{
	/* Close pipes. */
	msgbuf_write(&iev_engine->ibuf.w);
	msgbuf_clear(&iev_engine->ibuf.w);
	close(iev_engine->ibuf.fd);
	msgbuf_write(&iev_main->ibuf.w);
	msgbuf_clear(&iev_main->ibuf.w);
	close(iev_main->ibuf.fd);

	free(iev_engine);
	free(iev_main);

	log_info("frontend exiting");
	exit(0);
}

int
frontend_imsg_compose_main(int type, pid_t pid, void *data,
    uint16_t datalen)
{
	return (imsg_compose_event(iev_main, type, 0, pid, -1, data,
	    datalen));
}

int
frontend_imsg_compose_engine(int type, uint32_t peerid, pid_t pid,
    void *data, uint16_t datalen)
{
	return (imsg_compose_event(iev_engine, type, peerid, pid, -1,
	    data, datalen));
}

void
frontend_dispatch_main(int fd, short event, void *bula)
{
	struct imsg		 imsg;
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf = &iev->ibuf;
	struct iface		*iface;
	ssize_t			 n;
	int			 shut = 0, bpfsock, if_index, udpsock;

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
			 * Setup pipe and event handler to the engine
			 * process.
			 */
			if (iev_engine)
				fatalx("%s: received unexpected imsg fd "
				    "to frontend", __func__);

			if ((fd = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg fd to "
				   "frontend but didn't receive any",
				   __func__);

			iev_engine = malloc(sizeof(struct imsgev));
			if (iev_engine == NULL)
				fatal(NULL);

			imsg_init(&iev_engine->ibuf, fd);
			iev_engine->handler = frontend_dispatch_engine;
			iev_engine->events = EV_READ;

			event_set(&iev_engine->ev, iev_engine->ibuf.fd,
			iev_engine->events, iev_engine->handler, iev_engine);
			event_add(&iev_engine->ev, NULL);
			break;
		case IMSG_BPFSOCK:
			if ((bpfsock = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg "
				    "bpf fd but didn't receive any",
				    __func__);
			if (IMSG_DATA_SIZE(imsg) != sizeof(if_index))
				fatalx("%s: IMSG_BPFSOCK wrong length: "
				    "%lu", __func__, IMSG_DATA_SIZE(imsg));
			memcpy(&if_index, imsg.data, sizeof(if_index));
			set_bpfsock(bpfsock, if_index);
			break;
		case IMSG_UDPSOCK:
			if ((udpsock = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg "
				    "udpsocket fd but didn't receive any",
				    __func__);
			if (IMSG_DATA_SIZE(imsg) != sizeof(if_index))
				fatalx("%s: IMSG_UDPSOCK wrong length: "
				    "%lu", __func__, IMSG_DATA_SIZE(imsg));
			memcpy(&if_index, imsg.data, sizeof(if_index));
			if ((iface = get_iface_by_id(if_index)) == NULL) {
				close(fd);
				break;
			}
			if (iface->udpsock != -1)
				fatalx("%s: received unexpected udpsocket",
				    __func__);
			iface->udpsock = udpsock;
			break;
		case IMSG_CLOSE_UDPSOCK:
			if (IMSG_DATA_SIZE(imsg) != sizeof(if_index))
				fatalx("%s: IMSG_UDPSOCK wrong length: "
				    "%lu", __func__, IMSG_DATA_SIZE(imsg));
			memcpy(&if_index, imsg.data, sizeof(if_index));
			if ((iface = get_iface_by_id(if_index)) != NULL &&
			    iface->udpsock != -1) {
				close(iface->udpsock);
				iface->udpsock = -1;
			}
			break;
		case IMSG_ROUTESOCK:
			if ((fd = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg "
				    "routesocket fd but didn't receive any",
				    __func__);
			event_set(&ev_route, fd, EV_READ | EV_PERSIST,
			    route_receive, NULL);
			break;
		case IMSG_STARTUP:
			frontend_startup();
			break;
#ifndef	SMALL
		case IMSG_CONTROLFD:
			if ((fd = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg "
				    "control fd but didn't receive any",
				    __func__);
			/* Listen on control socket. */
			control_listen(fd);
			break;
		case IMSG_CTL_END:
			control_imsg_relay(&imsg);
			break;
#endif	/* SMALL */
		default:
			log_debug("%s: error handling imsg %d", __func__,
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
frontend_dispatch_engine(int fd, short event, void *bula)
{
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf = &iev->ibuf;
	struct imsg		 imsg;
	struct iface		*iface;
	ssize_t			 n;
	int			 shut = 0;

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
#ifndef	SMALL
		case IMSG_CTL_END:
		case IMSG_CTL_SHOW_INTERFACE_INFO:
			control_imsg_relay(&imsg);
			break;
#endif	/* SMALL */
		case IMSG_SEND_DISCOVER: {
			struct imsg_req_discover	 imsg_req_discover;
			if (IMSG_DATA_SIZE(imsg) != sizeof(imsg_req_discover))
				fatalx("%s: IMSG_SEND_DISCOVER wrong "
				    "length: %lu", __func__,
				    IMSG_DATA_SIZE(imsg));
			memcpy(&imsg_req_discover, imsg.data,
			    sizeof(imsg_req_discover));
			iface = get_iface_by_id(imsg_req_discover.if_index);
			if (iface != NULL) {
				iface->xid = imsg_req_discover.xid;
				send_discover(iface);
			}
			break;
		}
		case IMSG_SEND_REQUEST: {
			struct imsg_req_request	 imsg_req_request;
			if (IMSG_DATA_SIZE(imsg) != sizeof(imsg_req_request))
				fatalx("%s: IMSG_SEND_REQUEST wrong "
				    "length: %lu", __func__,
				    IMSG_DATA_SIZE(imsg));
			memcpy(&imsg_req_request, imsg.data,
			    sizeof(imsg_req_request));
			iface = get_iface_by_id(imsg_req_request.if_index);
			if (iface != NULL) {
				iface->xid = imsg_req_request.xid;
				iface->requested_ip.s_addr =
				    imsg_req_request.requested_ip.s_addr;
				iface->server_identifier.s_addr =
				    imsg_req_request.server_identifier.s_addr;
				iface->dhcp_server.s_addr =
				    imsg_req_request.dhcp_server.s_addr;
				send_request(iface);
			}
			break;
		}
		default:
			log_debug("%s: error handling imsg %d", __func__,
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

int
get_flags(char *if_name)
{
	struct ifreq		 ifr;

	strlcpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));
	if (ioctl(ioctlsock, SIOCGIFFLAGS, (caddr_t)&ifr) == -1) {
		log_warn("SIOCGIFFLAGS");
		return -1;
	}
	return ifr.ifr_flags;
}

int
get_xflags(char *if_name)
{
	struct ifreq		 ifr;

	strlcpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));
	if (ioctl(ioctlsock, SIOCGIFXFLAGS, (caddr_t)&ifr) == -1) {
		log_warn("SIOCGIFXFLAGS");
		return -1;
	}
	return ifr.ifr_flags;
}

void
update_iface(struct if_msghdr *ifm, struct sockaddr_dl *sdl)
{
	struct iface		*iface;
	struct imsg_ifinfo	 ifinfo;
	uint32_t		 if_index;
	int			 flags, xflags;
	char			 ifnamebuf[IF_NAMESIZE], *if_name;

	if_index = ifm->ifm_index;

	flags = ifm->ifm_flags;
	xflags = ifm->ifm_xflags;

	iface = get_iface_by_id(if_index);
	if_name = if_indextoname(if_index, ifnamebuf);

	if (if_name == NULL) {
		if (iface != NULL) {
			log_debug("interface with idx %d removed", if_index);
			frontend_imsg_compose_engine(IMSG_REMOVE_IF, 0, 0,
			    &if_index, sizeof(if_index));
			remove_iface(if_index);
		}
		return;
	}

	if (!(xflags & IFXF_AUTOCONF4)) {
		if (iface != NULL) {
			log_info("Removed autoconf flag from %s", if_name);
			frontend_imsg_compose_engine(IMSG_REMOVE_IF, 0, 0,
			    &if_index, sizeof(if_index));
			remove_iface(if_index);
		}
		return;
	}

	memset(&ifinfo, 0, sizeof(ifinfo));
	ifinfo.if_index = if_index;
	ifinfo.link_state = ifm->ifm_data.ifi_link_state;
	ifinfo.rdomain = ifm->ifm_tableid;
	ifinfo.running = (flags & (IFF_UP | IFF_RUNNING)) ==
	    (IFF_UP | IFF_RUNNING);

	if (sdl != NULL && sdl->sdl_type == IFT_ETHER &&
	    sdl->sdl_alen == ETHER_ADDR_LEN)
		memcpy(ifinfo.hw_address.ether_addr_octet, LLADDR(sdl),
		    ETHER_ADDR_LEN);
	else if (iface == NULL) {
		log_warnx("Could not find AF_LINK address for %s.", if_name);
		return;
	}

	if (iface == NULL) {
		if ((iface = calloc(1, sizeof(*iface))) == NULL)
			fatal("calloc");
		iface->udpsock = -1;
		LIST_INSERT_HEAD(&interfaces, iface, entries);
		frontend_imsg_compose_main(IMSG_OPEN_BPFSOCK, 0,
		    &if_index, sizeof(if_index));
	} else {
		if (iface->ifinfo.rdomain != ifinfo.rdomain &&
		    iface->udpsock != -1) {
			close(iface->udpsock);
			iface->udpsock = -1;
		}
	}

	if (memcmp(&iface->ifinfo, &ifinfo, sizeof(iface->ifinfo)) != 0) {
		memcpy(&iface->ifinfo, &ifinfo, sizeof(iface->ifinfo));
		frontend_imsg_compose_main(IMSG_UPDATE_IF, 0, &iface->ifinfo,
		    sizeof(iface->ifinfo));
	}
}

void
frontend_startup(void)
{
	if (!event_initialized(&ev_route))
		fatalx("%s: did not receive a route socket from the main "
		    "process", __func__);

	init_ifaces();
	if (pledge("stdio unix recvfd", NULL) == -1)
		fatal("pledge");
	event_add(&ev_route, NULL);
}

void
init_ifaces(void)
{
	struct iface		*iface;
	struct imsg_ifinfo	 ifinfo;
	struct if_nameindex	*ifnidxp, *ifnidx;
	struct ifaddrs		*ifap, *ifa;
	uint32_t		 if_index;
	int			 flags, xflags;
	char			*if_name;

	if ((ifnidxp = if_nameindex()) == NULL)
		fatalx("if_nameindex");

	if (getifaddrs(&ifap) != 0)
		fatal("getifaddrs");

	for (ifnidx = ifnidxp; ifnidx->if_index != 0 && ifnidx->if_name != NULL;
	    ifnidx++) {
		if_index = ifnidx->if_index;
		if_name = ifnidx->if_name;
		if ((flags = get_flags(if_name)) == -1)
			continue;
		if ((xflags = get_xflags(if_name)) == -1)
			continue;
		if (!(xflags & IFXF_AUTOCONF4))
			continue;

		memset(&ifinfo, 0, sizeof(ifinfo));
		ifinfo.if_index = if_index;
		ifinfo.link_state = -1;
		ifinfo.running = (flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING);

		for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
			if (strcmp(if_name, ifa->ifa_name) != 0)
				continue;
			if (ifa->ifa_addr == NULL)
				continue;

			switch (ifa->ifa_addr->sa_family) {
			case AF_LINK: {
				struct if_data		*if_data;
				struct sockaddr_dl	*sdl;

				sdl = (struct sockaddr_dl *)ifa->ifa_addr;
				if (sdl->sdl_type != IFT_ETHER ||
				    sdl->sdl_alen != ETHER_ADDR_LEN)
					continue;
				memcpy(ifinfo.hw_address.ether_addr_octet,
				    LLADDR(sdl), ETHER_ADDR_LEN);

				if_data = (struct if_data *)ifa->ifa_data;
				ifinfo.link_state = if_data->ifi_link_state;
				ifinfo.rdomain = if_data->ifi_rdomain;
				goto out;
			}
			default:
				break;
			}
		}
 out:
		if (ifinfo.link_state == -1)
			/* no AF_LINK found */
			continue;

		if ((iface = calloc(1, sizeof(*iface))) == NULL)
			fatal("calloc");
		iface->udpsock = -1;
		memcpy(&iface->ifinfo, &ifinfo, sizeof(iface->ifinfo));
		LIST_INSERT_HEAD(&interfaces, iface, entries);
		frontend_imsg_compose_main(IMSG_OPEN_BPFSOCK, 0,
		    &if_index, sizeof(if_index));
		frontend_imsg_compose_main(IMSG_UPDATE_IF, 0, &iface->ifinfo,
		    sizeof(iface->ifinfo));
	}

	freeifaddrs(ifap);
	if_freenameindex(ifnidxp);
}

void
route_receive(int fd, short events, void *arg)
{
	static uint8_t			 *buf;

	struct rt_msghdr		*rtm;
	struct sockaddr			*sa, *rti_info[RTAX_MAX];
	ssize_t				 n;

	if (buf == NULL) {
		buf = malloc(ROUTE_SOCKET_BUF_SIZE);
		if (buf == NULL)
			fatal("malloc");
	}
	rtm = (struct rt_msghdr *)buf;
	if ((n = read(fd, buf, ROUTE_SOCKET_BUF_SIZE)) == -1) {
		if (errno == EAGAIN || errno == EINTR)
			return;
		log_warn("dispatch_rtmsg: read error");
		return;
	}

	if (n == 0)
		fatal("routing socket closed");

	if (n < (ssize_t)sizeof(rtm->rtm_msglen) || n < rtm->rtm_msglen) {
		log_warnx("partial rtm of %zd in buffer", n);
		return;
	}

	if (rtm->rtm_version != RTM_VERSION)
		return;

	sa = (struct sockaddr *)(buf + rtm->rtm_hdrlen);
	get_rtaddrs(rtm->rtm_addrs, sa, rti_info);

	handle_route_message(rtm, rti_info);
}

void
handle_route_message(struct rt_msghdr *rtm, struct sockaddr **rti_info)
{
	struct sockaddr_dl	*sdl = NULL;
	switch (rtm->rtm_type) {
	case RTM_IFINFO:
		if (rtm->rtm_addrs & RTA_IFP && rti_info[RTAX_IFP]->sa_family
		    == AF_LINK)
			sdl = (struct sockaddr_dl *)rti_info[RTAX_IFP];
		update_iface((struct if_msghdr *)rtm, sdl);
		break;
	case RTM_PROPOSAL:
		if (rtm->rtm_priority == RTP_PROPOSAL_SOLICIT) {
			log_debug("RTP_PROPOSAL_SOLICIT");
			frontend_imsg_compose_engine(IMSG_REPROPOSE_RDNS,
			    0, 0, NULL, 0);
		}
		break;
	default:
		log_debug("unexpected RTM: %d", rtm->rtm_type);
		break;
	}
}

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

void
get_rtaddrs(int addrs, struct sockaddr *sa, struct sockaddr **rti_info)
{
	int	i;

	for (i = 0; i < RTAX_MAX; i++) {
		if (addrs & (1 << i)) {
			rti_info[i] = sa;
			sa = (struct sockaddr *)((char *)(sa) +
			    ROUNDUP(sa->sa_len));
		} else
			rti_info[i] = NULL;
	}
}

void
bpf_receive(int fd, short events, void *arg)
{
	struct bpf_hdr		*hdr;
	struct imsg_dhcp	 imsg_dhcp;
	struct iface		*iface;
	ssize_t			 len, rem;
	uint8_t			*p;

	iface = (struct iface *)arg;

	if ((len = read(fd, iface->bpfev.buf, BPFLEN)) == -1) {
		log_warn("read");
		return;
	}

	if (len == 0)
		fatal("%s len == 0", __func__);

	memset(&imsg_dhcp, 0, sizeof(imsg_dhcp));
	imsg_dhcp.if_index = iface->ifinfo.if_index;

	rem = len;
	p = iface->bpfev.buf;

	while (rem > 0) {
		if ((size_t)rem < sizeof(*hdr)) {
			log_warnx("packet too short");
			return;
		}
		hdr = (struct bpf_hdr *)p;
		if (hdr->bh_caplen != hdr->bh_datalen) {
			log_warnx("skipping truncated packet");
			goto cont;
		}
		if (rem < hdr->bh_hdrlen + hdr->bh_caplen)
			/* we are done */
			break;
		if (hdr->bh_caplen > sizeof(imsg_dhcp.packet)) {
			log_warn("packet too big");
			goto cont;
		}
		memcpy(&imsg_dhcp.packet, p + hdr->bh_hdrlen, hdr->bh_caplen);
		imsg_dhcp.len = hdr->bh_caplen;
		frontend_imsg_compose_engine(IMSG_DHCP, 0, 0, &imsg_dhcp,
		    sizeof(imsg_dhcp));
 cont:
		p += BPF_WORDALIGN(hdr->bh_hdrlen + hdr->bh_caplen);
		rem -= BPF_WORDALIGN(hdr->bh_hdrlen + hdr->bh_caplen);

	}
}

ssize_t
build_packet(uint8_t message_type, uint32_t xid, struct ether_addr *hw_address,
    struct in_addr *requested_ip, struct in_addr *server_identifier)
{
	static uint8_t	 dhcp_cookie[] = DHCP_COOKIE;
	static uint8_t	 dhcp_message_type[] = {DHO_DHCP_MESSAGE_TYPE, 1,
		DHCPDISCOVER};
	static uint8_t	 dhcp_hostname[255] = {DHO_HOST_NAME, 0 /*, ... */};
	static uint8_t	 dhcp_client_id[] = {DHO_DHCP_CLIENT_IDENTIFIER, 7,
		HTYPE_ETHER, 0, 0, 0, 0, 0, 0};
	static uint8_t	 dhcp_req_list[] = {DHO_DHCP_PARAMETER_REQUEST_LIST,
		7, DHO_SUBNET_MASK, DHO_ROUTERS, DHO_DOMAIN_NAME_SERVERS,
		DHO_HOST_NAME, DHO_DOMAIN_NAME, DHO_BROADCAST_ADDRESS,
		DHO_DOMAIN_SEARCH};
	static uint8_t	 dhcp_requested_address[] = {DHO_DHCP_REQUESTED_ADDRESS,
		4, 0, 0, 0, 0};
	static uint8_t	 dhcp_server_identifier[] = {DHO_DHCP_SERVER_IDENTIFIER,
		4, 0, 0, 0, 0};
	struct dhcp_hdr	*hdr;
	ssize_t		 len;
	uint8_t		*p;
	char		*c;

	memset(dhcp_packet, 0, sizeof(dhcp_packet));
	dhcp_message_type[2] = message_type;
	p = dhcp_packet;
	hdr = (struct dhcp_hdr *)p;
	hdr->op = DHCP_BOOTREQUEST;
	hdr->htype = HTYPE_ETHER;
	hdr->hlen = 6;
	hdr->hops = 0;
	hdr->xid = xid;
	hdr->secs = 0;
	memcpy(hdr->chaddr, hw_address, sizeof(*hw_address));
	p += sizeof(struct dhcp_hdr);
	memcpy(p, dhcp_cookie, sizeof(dhcp_cookie));
	p += sizeof(dhcp_cookie);
	memcpy(p, dhcp_message_type, sizeof(dhcp_message_type));
	p += sizeof(dhcp_message_type);
	if (gethostname(dhcp_hostname + 2, sizeof(dhcp_hostname) - 2) == 0) {
		if ((c = strchr(dhcp_hostname + 2, '.')) != NULL)
			*c = '\0';
		dhcp_hostname[1] = strlen(dhcp_hostname + 2);
		memcpy(p, dhcp_hostname, dhcp_hostname[1] + 2);
		p += dhcp_hostname[1] + 2;
	}
	memcpy(dhcp_client_id + 3, hw_address, sizeof(*hw_address));
	memcpy(p, dhcp_client_id, sizeof(dhcp_client_id));
	p += sizeof(dhcp_client_id);
	memcpy(p, dhcp_req_list, sizeof(dhcp_req_list));
	p += sizeof(dhcp_req_list);

	if (message_type == DHCPREQUEST) {
		memcpy(dhcp_requested_address + 2, requested_ip,
		    sizeof(*requested_ip));
		memcpy(p, dhcp_requested_address,
		    sizeof(dhcp_requested_address));
		p += sizeof(dhcp_requested_address);

		if (server_identifier->s_addr != INADDR_ANY) {
			memcpy(dhcp_server_identifier + 2, server_identifier,
			    sizeof(*server_identifier));
			memcpy(p, dhcp_server_identifier,
			    sizeof(dhcp_server_identifier));
			p += sizeof(dhcp_server_identifier);
		}
	}

	*p = DHO_END;
	p += 1;

	len = p - dhcp_packet;

	/* dhcp_packet is initialized with DHO_PADs */
	if (len < BOOTP_MIN_LEN)
		len = BOOTP_MIN_LEN;

	return (len);
}

void
send_discover(struct iface *iface)
{
	ssize_t	 pkt_len;
	char	 ifnamebuf[IF_NAMESIZE], *if_name;

	if (!event_initialized(&iface->bpfev.ev)) {
		iface->send_discover = 1;
		return;
	}
	iface->send_discover = 0;

	if_name = if_indextoname(iface->ifinfo.if_index, ifnamebuf);
	log_debug("DHCPDISCOVER on %s", if_name == NULL ? "?" : if_name);

	pkt_len = build_packet(DHCPDISCOVER, iface->xid,
	    &iface->ifinfo.hw_address, &iface->requested_ip, NULL);
	bpf_send_packet(iface, dhcp_packet, pkt_len);
}

void
send_request(struct iface *iface)
{
	ssize_t	 pkt_len;
	char	 ifnamebuf[IF_NAMESIZE], *if_name;

	if_name = if_indextoname(iface->ifinfo.if_index, ifnamebuf);
	log_debug("DHCPREQUEST on %s", if_name == NULL ? "?" : if_name);

	pkt_len = build_packet(DHCPREQUEST, iface->xid,
	    &iface->ifinfo.hw_address, &iface->requested_ip,
	    &iface->server_identifier);
	if (iface->dhcp_server.s_addr != INADDR_ANY)
		udp_send_packet(iface, dhcp_packet, pkt_len);
	else
		bpf_send_packet(iface, dhcp_packet, pkt_len);
}

void
udp_send_packet(struct iface *iface, uint8_t *packet, ssize_t len)
{
	struct sockaddr_in	to;

	memset(&to, 0, sizeof(to));
	to.sin_family = AF_INET;
	to.sin_len = sizeof(to);
	to.sin_addr.s_addr = iface->dhcp_server.s_addr;
	to.sin_port = ntohs(SERVER_PORT);

	if (sendto(iface->udpsock, packet, len, 0, (struct sockaddr *)&to,
	    sizeof(to)) == -1)
		log_warn("sendto");
}
void
bpf_send_packet(struct iface *iface, uint8_t *packet, ssize_t len)
{
	struct iovec		 iov[4];
	struct ether_header	 eh;
	struct ip		 ip;
	struct udphdr		 udp;
	ssize_t			 total, result;
	int			 iovcnt = 0, i;

	memset(eh.ether_dhost, 0xff, sizeof(eh.ether_dhost));
	memcpy(eh.ether_shost, &iface->ifinfo.hw_address,
	    sizeof(eh.ether_dhost));
	eh.ether_type = htons(ETHERTYPE_IP);
	iov[0].iov_base = &eh;
	iov[0].iov_len = sizeof(eh);
	iovcnt++;

	ip.ip_v = 4;
	ip.ip_hl = 5;
	ip.ip_tos = IPTOS_LOWDELAY;
	ip.ip_len = htons(sizeof(ip) + sizeof(udp) + len);
	ip.ip_id = 0;
	ip.ip_off = 0;
	ip.ip_ttl = 128;
	ip.ip_p = IPPROTO_UDP;
	ip.ip_sum = 0;
	ip.ip_src.s_addr = 0;
	ip.ip_dst.s_addr = INADDR_BROADCAST;
	ip.ip_sum = wrapsum(checksum((unsigned char *)&ip, sizeof(ip), 0));
	iov[iovcnt].iov_base = &ip;
	iov[iovcnt].iov_len = sizeof(ip);
	iovcnt++;

	udp.uh_sport = htons(CLIENT_PORT);
	udp.uh_dport = htons(SERVER_PORT);
	udp.uh_ulen = htons(sizeof(udp) + len);
	udp.uh_sum = 0;
	udp.uh_sum = wrapsum(checksum((unsigned char *)&udp, sizeof(udp),
	    checksum((unsigned char *)packet, len,
	    checksum((unsigned char *)&ip.ip_src,
	    2 * sizeof(ip.ip_src),
	    IPPROTO_UDP + (uint32_t)ntohs(udp.uh_ulen)))));
	iov[iovcnt].iov_base = &udp;
	iov[iovcnt].iov_len = sizeof(udp);
	iovcnt++;

	iov[iovcnt].iov_base = packet;
	iov[iovcnt].iov_len = len;
	iovcnt++;

	total = 0;
	for (i = 0; i < iovcnt; i++)
		total += iov[i].iov_len;

	result = writev(EVENT_FD(&iface->bpfev.ev), iov, iovcnt);
	if (result == -1)
		log_warn("%s: writev", __func__);
	else if (result < total) {
		log_warnx("%s, writev: %zd of %zd bytes", __func__, result,
		    total);
	}
}

struct iface*
get_iface_by_id(uint32_t if_index)
{
	struct iface	*iface;

	LIST_FOREACH (iface, &interfaces, entries) {
		if (iface->ifinfo.if_index == if_index)
			return (iface);
	}

	return (NULL);
}

void
remove_iface(uint32_t if_index)
{
	struct iface	*iface;

	iface = get_iface_by_id(if_index);

	if (iface == NULL)
		return;

	LIST_REMOVE(iface, entries);
	event_del(&iface->bpfev.ev);
	close(EVENT_FD(&iface->bpfev.ev));
	if (iface->udpsock != -1)
		close(iface->udpsock);
	free(iface);
}

void
set_bpfsock(int bpfsock, uint32_t if_index)
{
	struct iface	*iface;

	if ((iface = get_iface_by_id(if_index)) == NULL) {
		/*
		 * The interface disappeared while we were waiting for the
		 * parent process to open the raw socket.
		 */
		close(bpfsock);
	} else {
		event_set(&iface->bpfev.ev, bpfsock, EV_READ |
		    EV_PERSIST, bpf_receive, iface);
		event_add(&iface->bpfev.ev, NULL);
		if (iface->send_discover)
			send_discover(iface);
	}
}
