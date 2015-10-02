/*	$OpenBSD$ */

/*
 * Copyright (c) 2015 Renato Westphal <renato@openbsd.org>
 * Copyright (c) 2005 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2004, 2005 Esben Norby <norby@openbsd.org>
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

#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <err.h>
#include <string.h>

#include "eigrpd.h"
#include "eigrp.h"
#include "log.h"
#include "eigrpe.h"

extern struct eigrpd_conf        *econf;

void		 eigrp_if_hello_timer(int, short, void *);
void		 eigrp_if_start_hello_timer(struct eigrp_iface *);
void		 eigrp_if_stop_hello_timer(struct eigrp_iface *);

static __inline int iface_id_compare(struct eigrp_iface *,
    struct eigrp_iface *);

RB_HEAD(iface_id_head, eigrp_iface);
RB_PROTOTYPE(iface_id_head, eigrp_iface, id_tree, iface_id_compare)
RB_GENERATE(iface_id_head, eigrp_iface, id_tree, iface_id_compare)

static __inline int
iface_id_compare(struct eigrp_iface *a, struct eigrp_iface *b)
{
	return (a->ifaceid - b->ifaceid);
}

struct iface_id_head ifaces_by_id = RB_INITIALIZER(&ifaces_by_id);

uint32_t	ifacecnt = 1;

struct iface *
if_new(struct eigrpd_conf *xconf, struct kif *kif)
{
	struct iface		*iface;

	if ((iface = calloc(1, sizeof(*iface))) == NULL)
		err(1, "if_new: calloc");

	TAILQ_INIT(&iface->ei_list);
	TAILQ_INIT(&iface->addr_list);

	strlcpy(iface->name, kif->ifname, sizeof(iface->name));

	/* get type */
	if (kif->flags & IFF_POINTOPOINT)
		iface->type = IF_TYPE_POINTOPOINT;
	if (kif->flags & IFF_BROADCAST &&
	    kif->flags & IFF_MULTICAST)
		iface->type = IF_TYPE_BROADCAST;
	if (kif->flags & IFF_LOOPBACK)
		iface->type = IF_TYPE_POINTOPOINT;

	/* get index and flags */
	iface->mtu = kif->mtu;
	iface->ifindex = kif->ifindex;
	iface->flags = kif->flags;
	iface->linkstate = kif->link_state;
	iface->if_type = kif->if_type;
	iface->baudrate = kif->baudrate;

	TAILQ_INSERT_TAIL(&xconf->iface_list, iface, entry);

	return (iface);
}

void
if_del(struct iface *iface)
{
	struct eigrp_iface	*ei;
	struct if_addr		*if_addr;

	log_debug("%s: interface %s", __func__, iface->name);

	while ((ei = TAILQ_FIRST(&iface->ei_list)) != NULL)
		eigrp_if_del(ei);

	while ((if_addr = TAILQ_FIRST(&iface->addr_list)) != NULL) {
		TAILQ_REMOVE(&iface->addr_list, if_addr, entry);
		free(if_addr);
	}

	TAILQ_REMOVE(&econf->iface_list, iface, entry);
	free(iface);
}

void
if_init(struct eigrpd_conf *xconf, struct iface *iface)
{
	struct ifreq		 ifr;
	unsigned int		 rdomain;
	struct eigrp_iface	*ei;
	union eigrpd_addr	 addr;

	memset(&addr, 0, sizeof(addr));
	TAILQ_FOREACH(ei, &iface->ei_list, i_entry) {
		/* init the dummy self neighbor */
		ei->self = nbr_new(ei, &addr, 0, 1);
		nbr_init(ei->self);

		/* set event handlers for interface */
		evtimer_set(&ei->hello_timer, eigrp_if_hello_timer, ei);
	}

	/* set rdomain */
	strlcpy(ifr.ifr_name, iface->name, sizeof(ifr.ifr_name));
	if (ioctl(econf->eigrp_socket_v4, SIOCGIFRDOMAIN, (caddr_t)&ifr) == -1)
		rdomain = 0;
	else {
		rdomain = ifr.ifr_rdomainid;
		if (setsockopt(econf->eigrp_socket_v4, SOL_SOCKET, SO_RTABLE,
		    &rdomain, sizeof(rdomain)) == -1)
			fatal("failed to set rdomain");
	}
	if (rdomain != xconf->rdomain)
		fatalx("interface rdomain mismatch");
}

struct iface *
if_lookup(struct eigrpd_conf *xconf, unsigned int ifindex)
{
	struct iface	*iface;

	TAILQ_FOREACH(iface, &xconf->iface_list, entry)
		if (iface->ifindex == ifindex)
			return (iface);

	return (NULL);
}

struct if_addr *
if_addr_new(struct iface *iface, struct kaddr *kaddr)
{
	struct if_addr		*if_addr;
	struct eigrp_iface	*ei;

	if (if_addr_lookup(&iface->addr_list, kaddr) != NULL)
		return (NULL);

	if ((if_addr = calloc(1, sizeof(*if_addr))) == NULL)
		fatal("if_addr_new");

	if_addr->af = kaddr->af;
	memcpy(&if_addr->addr, &kaddr->addr, sizeof(if_addr->addr));
	if_addr->prefixlen = kaddr->prefixlen;
	memcpy(&if_addr->dstbrd, &kaddr->dstbrd, sizeof(if_addr->dstbrd));

	TAILQ_INSERT_TAIL(&iface->addr_list, if_addr, entry);

	TAILQ_FOREACH(ei, &iface->ei_list, i_entry)
		if (ei->state == IF_STA_ACTIVE && ei->eigrp->af == if_addr->af)
			eigrpe_orig_local_route(ei, if_addr, 0);

	if_update(iface, if_addr->af);

	return (if_addr);
}

void
if_addr_del(struct iface *iface, struct kaddr *kaddr)
{
	struct if_addr		*if_addr;
	struct eigrp_iface	*ei;

	if_addr = if_addr_lookup(&iface->addr_list, kaddr);
	if (if_addr == NULL)
		return;

	TAILQ_FOREACH(ei, &iface->ei_list, i_entry)
		if (ei->state == IF_STA_ACTIVE && ei->eigrp->af == if_addr->af)
			eigrpe_orig_local_route(ei, if_addr, 1);

	TAILQ_REMOVE(&iface->addr_list, if_addr, entry);
	if_update(iface, if_addr->af);
	free(if_addr);
}

struct if_addr *
if_addr_lookup(struct if_addr_head *addr_list, struct kaddr *kaddr)
{
	struct if_addr	*if_addr;
	int		 af = kaddr->af;

	TAILQ_FOREACH(if_addr, addr_list, entry)
		if (!eigrp_addrcmp(af, &if_addr->addr, &kaddr->addr) &&
		    if_addr->prefixlen == kaddr->prefixlen &&
		    !eigrp_addrcmp(af, &if_addr->dstbrd, &kaddr->dstbrd))
			return (if_addr);

	return (NULL);
}

in_addr_t
if_primary_addr(struct iface *iface)
{
	struct if_addr	*if_addr;

	TAILQ_FOREACH(if_addr, &iface->addr_list, entry)
		if (if_addr->af == AF_INET)
			return (if_addr->addr.v4.s_addr);

	return (INADDR_ANY);
}

uint8_t
if_primary_addr_prefixlen(struct iface *iface)
{
	struct if_addr	*if_addr;

	TAILQ_FOREACH(if_addr, &iface->addr_list, entry)
		if (if_addr->af == AF_INET)
			return (if_addr->prefixlen);

	return (0);
}

/* up/down events */
void
if_update(struct iface *iface, int af)
{
	struct eigrp_iface	*ei;
	int			 link_ok;

	link_ok = (iface->flags & IFF_UP) &&
	    LINK_STATE_IS_UP(iface->linkstate);

	TAILQ_FOREACH(ei, &iface->ei_list, i_entry) {
		if (af != AF_UNSPEC && ei->eigrp->af != af)
			continue;

		if (ei->state == IF_STA_DOWN) {
			if (!link_ok)
				continue;
			if (af == AF_INET && TAILQ_EMPTY(&iface->addr_list))
				continue;
			ei->state = IF_STA_ACTIVE;
			eigrp_if_start(ei);
		} else {
			if (link_ok)
				continue;
			if (!(af == AF_INET && TAILQ_EMPTY(&iface->addr_list)))
				continue;
			ei->state = IF_STA_DOWN;
			eigrp_if_reset(ei);
		}
	}
}

struct eigrp_iface *
eigrp_if_new(struct eigrpd_conf *xconf, struct eigrp *eigrp, struct kif *kif)
{
	struct iface		*iface;
	struct eigrp_iface	*ei;
	struct timeval		 now;

	iface = if_lookup(xconf, kif->ifindex);
	if (iface == NULL)
		iface = if_new(xconf, kif);

	if ((ei = calloc(1, sizeof(*ei))) == NULL)
		err(1, "eigrp_if_new: calloc");

	ei->state = IF_STA_DOWN;
	/* get next unused ifaceid */
	while (eigrp_iface_find_id(ifacecnt++))
		;
	ei->ifaceid = ifacecnt;
	ei->eigrp = eigrp;
	ei->iface = iface;
	if (ei->iface->flags & IFF_LOOPBACK)
		ei->passive = 1;

	gettimeofday(&now, NULL);
	ei->uptime = now.tv_sec;

	TAILQ_INIT(&ei->nbr_list);
	TAILQ_INIT(&ei->update_list);
	TAILQ_INIT(&ei->query_list);
	TAILQ_INSERT_TAIL(&iface->ei_list, ei, i_entry);
	TAILQ_INSERT_TAIL(&eigrp->ei_list, ei, e_entry);
	if (RB_INSERT(iface_id_head, &ifaces_by_id, ei) != NULL)
		fatalx("eigrp_if_new: RB_INSERT(ifaces_by_id) failed");

	return (ei);
}

void
eigrp_if_del(struct eigrp_iface *ei)
{
	struct nbr	*nbr;

	RB_REMOVE(iface_id_head, &ifaces_by_id, ei);
	TAILQ_REMOVE(&ei->eigrp->ei_list, ei, e_entry);
	TAILQ_REMOVE(&ei->iface->ei_list, ei, i_entry);

	if (ei->state == IF_STA_ACTIVE)
		eigrp_if_reset(ei);

	while ((nbr = TAILQ_FIRST(&ei->nbr_list)) != NULL)
		nbr_del(nbr);

	if (TAILQ_EMPTY(&ei->iface->ei_list))
		if_del(ei->iface);

	free(ei);
}

void
eigrp_if_start(struct eigrp_iface *ei)
{
	struct eigrp		*eigrp = ei->eigrp;
	struct if_addr		*if_addr;
	struct in_addr		 addr4;
	struct in6_addr		 addr6;

	log_debug("%s: %s as %u family %s", __func__, ei->iface->name,
	    eigrp->as, af_name(eigrp->af));

	TAILQ_FOREACH(if_addr, &ei->iface->addr_list, entry) {
		if (if_addr->af != eigrp->af)
			continue;

		eigrpe_orig_local_route(ei, if_addr, 0);
	}

	if (ei->passive)
		return;

	switch (eigrp->af) {
	case AF_INET:
		addr4.s_addr = AllEIGRPRouters_v4;
		if (if_join_ipv4_group(ei->iface, &addr4))
			return;
		break;
	case AF_INET6:
		inet_pton(AF_INET6, AllEIGRPRouters_v6, &addr6);
		if (if_join_ipv6_group(ei->iface, &addr6))
			return;
		break;
	default:
		break;
	}

	eigrp_if_start_hello_timer(ei);
}

void
eigrp_if_reset(struct eigrp_iface *ei)
{
	struct eigrp		*eigrp = ei->eigrp;
	struct in_addr		 addr4;
	struct in6_addr		 addr6;

	log_debug("%s: %s as %u family %s", __func__, ei->iface->name,
	    eigrp->as, af_name(eigrp->af));

	/* the rde will withdraw the connected route for us */

	if (ei->passive)
		return;

	/* try to cleanup */
	switch (eigrp->af) {
	case AF_INET:
		addr4.s_addr = AllEIGRPRouters_v4;
		if_leave_ipv4_group(ei->iface, &addr4);
		break;
	case AF_INET6:
		inet_pton(AF_INET6, AllEIGRPRouters_v6, &addr6);
		if_leave_ipv6_group(ei->iface, &addr6);
		break;
	default:
		break;
	}

	eigrp_if_stop_hello_timer(ei);
}

struct eigrp_iface *
eigrp_iface_find_id(uint32_t ifaceid)
{
	struct eigrp_iface	 e;
	e.ifaceid = ifaceid;
	return (RB_FIND(iface_id_head, &ifaces_by_id, &e));
}

struct eigrp_iface *
eigrp_if_lookup(struct iface *iface, int af, uint16_t as)
{
	struct eigrp_iface	*ei;

	TAILQ_FOREACH(ei, &iface->ei_list, i_entry)
		if (ei->eigrp->af == af &&
		    ei->eigrp->as == as)
			return (ei);

	return (NULL);
}

/* timers */
/* ARGSUSED */
void
eigrp_if_hello_timer(int fd, short event, void *arg)
{
	struct eigrp_iface	*ei = arg;
	struct timeval		 tv;

	send_hello(ei, NULL, 0);

	/* reschedule hello_timer */
	timerclear(&tv);
	tv.tv_sec = ei->hello_interval;
	if (evtimer_add(&ei->hello_timer, &tv) == -1)
		fatal("eigrp_if_hello_timer");
}

void
eigrp_if_start_hello_timer(struct eigrp_iface *ei)
{
	struct timeval		 tv;

	timerclear(&tv);
	tv.tv_sec = ei->hello_interval;
	if (evtimer_add(&ei->hello_timer, &tv) == -1)
		fatal("eigrp_if_start_hello_timer");
}

void
eigrp_if_stop_hello_timer(struct eigrp_iface *ei)
{
	if (evtimer_pending(&ei->hello_timer, NULL) &&
	    evtimer_del(&ei->hello_timer) == -1)
		fatal("eigrp_if_stop_hello_timer");
}

struct ctl_iface *
if_to_ctl(struct eigrp_iface *ei)
{
	static struct ctl_iface	 ictl;
	struct timeval		 now;
	struct nbr		*nbr;

	ictl.af = ei->eigrp->af;
	ictl.as = ei->eigrp->as;
	memcpy(ictl.name, ei->iface->name, sizeof(ictl.name));
	ictl.ifindex = ei->iface->ifindex;
	switch (ei->eigrp->af) {
	case AF_INET:
		ictl.addr.v4.s_addr = if_primary_addr(ei->iface);
		ictl.prefixlen = if_primary_addr_prefixlen(ei->iface);
		break;
	case AF_INET6:
		memcpy(&ictl.addr.v6, &ei->iface->linklocal,
		    sizeof(ictl.addr.v6));
		ictl.prefixlen = 64;
		break;
	default:
		break;
	}
	ictl.flags = ei->iface->flags;
	ictl.linkstate = ei->iface->linkstate;
	ictl.mtu = ei->iface->mtu;
	ictl.type = ei->iface->type;
	ictl.if_type = ei->iface->if_type;
	ictl.baudrate = ei->iface->baudrate;
	ictl.delay = ei->delay;
	ictl.bandwidth = ei->bandwidth;
	ictl.hello_holdtime = ei->hello_holdtime;
	ictl.hello_interval = ei->hello_interval;
	ictl.splithorizon = ei->splithorizon;
	ictl.passive = ei->passive;
	ictl.nbr_cnt = 0;

	gettimeofday(&now, NULL);
	if (ei->state != IF_STA_DOWN && ei->uptime != 0)
		ictl.uptime = now.tv_sec - ei->uptime;
	else
		ictl.uptime = 0;

	TAILQ_FOREACH(nbr, &ei->nbr_list, entry)
		if (!(nbr->flags & (F_EIGRP_NBR_PENDING|F_EIGRP_NBR_SELF)))
			ictl.nbr_cnt++;

	return (&ictl);
}

/* misc */
void
if_set_sockbuf(int fd)
{
	int	bsize;

	bsize = 65535;
	while (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &bsize,
	    sizeof(bsize)) == -1)
		bsize /= 2;

	if (bsize != 65535)
		log_warnx("%s: recvbuf size only %d", __func__, bsize);

	bsize = 65535;
	while (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &bsize,
	    sizeof(bsize)) == -1)
		bsize /= 2;

	if (bsize != 65535)
		log_warnx("%s: sendbuf size only %d", __func__, bsize);
}

int
if_join_ipv4_group(struct iface *iface, struct in_addr *addr)
{
	struct ip_mreq		 mreq;

	if (iface->group_count_v4++ != 0)
		/* already joined */
		return (0);

	log_debug("%s: interface %s addr %s", __func__, iface->name,
	    inet_ntoa(*addr));

	mreq.imr_multiaddr.s_addr = addr->s_addr;
	mreq.imr_interface.s_addr = if_primary_addr(iface);

	if (setsockopt(econf->eigrp_socket_v4, IPPROTO_IP, IP_ADD_MEMBERSHIP,
	    (void *)&mreq, sizeof(mreq)) < 0) {
		log_warn("%s: error IP_ADD_MEMBERSHIP, interface %s address %s",
		    __func__, iface->name, inet_ntoa(*addr));
		return (-1);
	}

	return (0);
}

int
if_leave_ipv4_group(struct iface *iface, struct in_addr *addr)
{
	struct ip_mreq		 mreq;

	if (--iface->group_count_v4 != 0)
		/* others still joined */
		return (0);

	log_debug("%s: interface %s addr %s", __func__, iface->name,
	    inet_ntoa(*addr));

	mreq.imr_multiaddr.s_addr = addr->s_addr;
	mreq.imr_interface.s_addr = if_primary_addr(iface);

	if (setsockopt(econf->eigrp_socket_v4, IPPROTO_IP, IP_DROP_MEMBERSHIP,
	    (void *)&mreq, sizeof(mreq)) < 0) {
		log_warn("%s: error IP_DROP_MEMBERSHIP, interface %s "
		    "address %s", iface->name, __func__, inet_ntoa(*addr));
		return (-1);
	}

	return (0);
}

int
if_set_ipv4_mcast_ttl(int fd, uint8_t ttl)
{
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL,
	    (char *)&ttl, sizeof(ttl)) < 0) {
		log_warn("%s: error setting IP_MULTICAST_TTL to %d",
		    __func__, ttl);
		return (-1);
	}

	return (0);
}

int
if_set_ipv4_mcast(struct iface *iface)
{
	in_addr_t	 addr;

	addr = if_primary_addr(iface);

	if (setsockopt(econf->eigrp_socket_v4, IPPROTO_IP, IP_MULTICAST_IF,
	    &addr, sizeof(addr)) < 0) {
		log_warn("%s: error setting IP_MULTICAST_IF, interface %s",
		    __func__, iface->name);
		return (-1);
	}

	return (0);
}

int
if_set_ipv4_mcast_loop(int fd)
{
	uint8_t	loop = 0;

	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP,
	    (char *)&loop, sizeof(loop)) < 0) {
		log_warn("%s: error setting IP_MULTICAST_LOOP", __func__);
		return (-1);
	}

	return (0);
}

int
if_set_ipv4_recvif(int fd, int enable)
{
	if (setsockopt(fd, IPPROTO_IP, IP_RECVIF, &enable,
	    sizeof(enable)) < 0) {
		log_warn("%s: error setting IP_RECVIF", __func__);
		return (-1);
	}
	return (0);
}

int
if_set_ipv4_hdrincl(int fd)
{
	int	hincl = 1;

	if (setsockopt(fd, IPPROTO_IP, IP_HDRINCL, &hincl, sizeof(hincl)) < 0) {
		log_warn("%s: error setting IP_HDRINCL", __func__);
		return (-1);
	}

	return (0);
}

int
if_join_ipv6_group(struct iface *iface, struct in6_addr *addr)
{
	struct ipv6_mreq	 mreq;

	if (iface->group_count_v6++ != 0)
		/* already joined */
		return (0);

	log_debug("%s: interface %s addr %s", __func__, iface->name,
	    log_in6addr(addr));

	mreq.ipv6mr_multiaddr = *addr;
	mreq.ipv6mr_interface = iface->ifindex;

	if (setsockopt(econf->eigrp_socket_v6, IPPROTO_IPV6, IPV6_JOIN_GROUP,
	    &mreq, sizeof(mreq)) < 0) {
		log_warn("%s: error IPV6_JOIN_GROUP, interface %s address %s",
		    __func__, iface->name, log_in6addr(addr));
		return (-1);
	}

	return (0);
}

int
if_leave_ipv6_group(struct iface *iface, struct in6_addr *addr)
{
	struct ipv6_mreq	 mreq;

	if (--iface->group_count_v6 != 0)
		/* others still joined */
		return (0);

	log_debug("%s: interface %s addr %s", __func__, iface->name,
	    log_in6addr(addr));

	mreq.ipv6mr_multiaddr = *addr;
	mreq.ipv6mr_interface = iface->ifindex;

	if (setsockopt(econf->eigrp_socket_v6, IPPROTO_IPV6, IPV6_LEAVE_GROUP,
	    (void *)&mreq, sizeof(mreq)) < 0) {
		log_warn("%s: error IPV6_LEAVE_GROUP, interface %s address %s",
		    __func__, iface->name, log_in6addr(addr));
		return (-1);
	}

	return (0);
}

int
if_set_ipv6_mcast(struct iface *iface)
{
	if (setsockopt(econf->eigrp_socket_v6, IPPROTO_IPV6, IPV6_MULTICAST_IF,
	    &iface->ifindex, sizeof(iface->ifindex)) < 0) {
		log_debug("%s: error setting IPV6_MULTICAST_IF, interface %s",
		    __func__, iface->name);
		return (-1);
	}

	return (0);
}

int
if_set_ipv6_mcast_loop(int fd)
{
	unsigned int	loop = 0;

	if (setsockopt(fd, IPPROTO_IPV6, IPV6_MULTICAST_LOOP,
	    (unsigned int *)&loop, sizeof(loop)) < 0) {
		log_warn("%s: error setting IPV6_MULTICAST_LOOP", __func__);
		return (-1);
	}

	return (0);
}

int
if_set_ipv6_pktinfo(int fd, int enable)
{
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &enable,
	    sizeof(enable)) < 0) {
		log_warn("%s: error setting IPV6_PKTINFO", __func__);
		return (-1);
	}

	return (0);
}

int
if_set_ipv6_dscp(int fd, int dscp)
{
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_TCLASS, &dscp,
	    sizeof(dscp)) < 0) {
		log_warn("%s: error setting IPV6_TCLASS", __func__);
		return (-1);
	}

	return (0);
}
