/*	$OpenBSD: kroute.c,v 1.26 2012/12/29 14:40:00 krw Exp $	*/

/*
 * Copyright 2012 Kenneth R Westerback <krw@openbsd.org>
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

#include "dhcpd.h"
#include "privsep.h"

#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <net/if_types.h>

#include <ifaddrs.h>

struct in_addr active_addr;

/*
 * Do equivalent of 
 *
 *	route -q $rdomain -n flush -inet -iface $interface
 *	arp -dan
 */
void
flush_routes_and_arp_cache(char *ifname, int rdomain)
{
	struct imsg_flush_routes imsg;
	int			 rslt;

	memset(&imsg, 0, sizeof(imsg));

	strlcpy(imsg.ifname, ifname, sizeof(imsg.ifname));
	imsg.rdomain = rdomain;

	rslt = imsg_compose(unpriv_ibuf, IMSG_FLUSH_ROUTES, 0, 0, -1,
	    &imsg, sizeof(imsg));
	if (rslt == -1)
		warning("flush_routes_and_arp_cache: imsg_compose: %s",
		    strerror(errno));

	/* Do flush to maximize chances of cleaning up routes on exit. */
	rslt = imsg_flush(unpriv_ibuf);
	if (rslt == -1)
		warning("flush_routes_and_arp_cache: imsg_flush: %s",
		    strerror(errno));
}

void
priv_flush_routes_and_arp_cache(struct imsg_flush_routes *imsg)
{
	char ifname[IF_NAMESIZE];
	struct sockaddr *rti_info[RTAX_MAX];
	int mib[7];
	size_t needed;
	char *lim, *buf, *next, *routelabel;
	struct rt_msghdr *rtm;
	struct sockaddr *sa;
	struct sockaddr_dl *sdl;
	struct sockaddr_in *sa_in;
	struct sockaddr_inarp *sin;
	struct sockaddr_rtlabel *sa_rl;
	int s, seqno = 0, rlen, i;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;
	mib[6] = imsg->rdomain;

	if (sysctl(mib, 7, NULL, &needed, NULL, 0) == -1) {
		if (imsg->rdomain != 0 && errno == EINVAL)
			return;
		error("could not get routes");
	}

	if (needed == 0)
		return;

	if ((buf = malloc(needed)) == NULL)
		error("malloc");

	if (sysctl(mib, 7, buf, &needed, NULL, 0) == -1)
		error("sysctl retrieval of routes: %s", strerror(errno));

	if ((s = socket(AF_ROUTE, SOCK_RAW, 0)) == -1)
		error("opening socket to flush routes: %s", strerror(errno));

	if (asprintf(&routelabel, "DHCLIENT %d", (int)getpid()) == -1)
		error("recreating route label: %s", strerror(errno));

#define ROUNDUP(a) \
    ((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

	lim = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		if (rtm->rtm_version != RTM_VERSION)
			continue;

		sa = (struct sockaddr *)(next + rtm->rtm_hdrlen);
		if (sa->sa_family == AF_KEY)
			continue;  /* Don't flush SPD */

		memset(rti_info, 0, sizeof(rti_info));
		for (i = 0; i < RTAX_MAX; i++) {
			if (rtm->rtm_addrs & (1 << i)) {
				rti_info[i] = sa;
				sa = (struct sockaddr *)((char *)(sa) +
				    ROUNDUP(sa->sa_len));
			}
		}

		sa = (struct sockaddr *)(next + rtm->rtm_hdrlen);

		sa_rl = (struct sockaddr_rtlabel *)rti_info[RTAX_LABEL];
		if (sa_rl) {
			/* Always delete routes we labeled. */
			if (strcmp(routelabel, sa_rl->sr_label) == 0)
				goto delete;

			/* Never delete routes labelled by another dhclient. */
			if (strlen(sa_rl->sr_label) > 8 &&
			    strncmp("DHCLIENT ", sa_rl->sr_label, 9) == 0)
				continue;
		}

		if (rtm->rtm_flags & RTF_LLINFO) {
			if (rtm->rtm_flags & RTF_GATEWAY)
				continue;

			/* XXXX Check for AF_INET too? (arp ask for them) */
			/* XXXX Need 'retry' for proxy entries? (arp does) */

			sin = (struct sockaddr_inarp *)(sa);
			sdl = (struct sockaddr_dl *)(ROUNDUP(sin->sin_len) +
			   (char *)sin);

			if (sdl->sdl_family == AF_LINK) {
				switch (sdl->sdl_type) {
				case IFT_ETHER:
				case IFT_FDDI:
				case IFT_ISO88023:
				case IFT_ISO88024:
				case IFT_ISO88025:
				case IFT_CARP:
					/* Delete it. */
					goto delete;
				default:
					break;
				}
			}
			continue;
		}

		if (rtm->rtm_flags & RTF_GATEWAY) {
			memset(ifname, 0, sizeof(ifname));
			if (if_indextoname(rtm->rtm_index, ifname) == NULL)
				continue;
			sa_in = (struct sockaddr_in *)rti_info[RTAX_NETMASK];
			if (sa_in && 
			    sa_in->sin_addr.s_addr == INADDR_ANY &&
			    rtm->rtm_tableid == imsg->rdomain &&
			    strcmp(imsg->ifname, ifname) == 0)
				goto delete;
		}

		continue;

delete:
		rtm->rtm_type = RTM_DELETE;
		rtm->rtm_seq = seqno;
		rtm->rtm_tableid = imsg->rdomain;

		rlen = write(s, next, rtm->rtm_msglen);
		if (rlen == -1) {
			if (errno != ESRCH)
				error("RTM_DELETE write: %s", strerror(errno));
		} else if (rlen < (int)rtm->rtm_msglen)
			error("short RTM_DELETE write (%d)\n", rlen);

		seqno++;
	}

	close(s);
	free(buf);
	free(routelabel);
}

/*
 * [priv_]add_default_route is the equivalent of
 *
 *	route -q $rdomain -n flush -inet -iface $interface
 *
 * and one of
 *
 *	route -q $rdomain add default -iface $router
 * 	route -q $rdomain add default $router
 *
 * depending on the contents of the gateway parameter.
 */
void
add_default_route(int rdomain, struct in_addr addr, struct in_addr gateway)
{
	struct imsg_add_default_route	 imsg;
	int				 rslt;

	memset(&imsg, 0, sizeof(imsg));

	imsg.rdomain = rdomain;
	imsg.addr = addr;
	imsg.gateway = gateway;

	rslt = imsg_compose(unpriv_ibuf, IMSG_ADD_DEFAULT_ROUTE, 0, 0, -1,
	    &imsg, sizeof(imsg));

	if (rslt == -1)
		warning("add_default_route: imsg_compose: %s", strerror(errno));
}

void
priv_add_default_route(struct imsg_add_default_route *imsg)
{
	struct rt_msghdr rtm;
	struct sockaddr_in dest, gateway, mask;
	struct sockaddr_rtlabel label;
	struct iovec iov[5];
	int s, len, i, iovcnt = 0;

	/*
	 * Add a default route via the specified address.
	 */

	if ((s = socket(AF_ROUTE, SOCK_RAW, 0)) == -1)
		error("Routing Socket open failed: %s", strerror(errno));

	/* Build RTM header */

	memset(&rtm, 0, sizeof(rtm));

	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_type = RTM_ADD;
	rtm.rtm_tableid = imsg->rdomain;
	rtm.rtm_priority = 0;
	rtm.rtm_msglen = sizeof(rtm);

	iov[iovcnt].iov_base = &rtm;
	iov[iovcnt++].iov_len = sizeof(rtm);
	
	/* Set destination address of all zeros. */

	memset(&dest, 0, sizeof(dest));

	dest.sin_len = sizeof(dest);
	dest.sin_family = AF_INET;

	rtm.rtm_addrs |= RTA_DST;
	rtm.rtm_msglen += sizeof(dest);

	iov[iovcnt].iov_base = &dest;
	iov[iovcnt++].iov_len = sizeof(dest);
	
	/*
	 * Set gateway address if and only if non-zero addr supplied. A
	 * gateway address of 0 implies '-iface'.
	 */

	memset(&gateway, 0, sizeof(gateway));
	if (bcmp(&imsg->gateway, &imsg->addr, sizeof(imsg->addr)) != 0) {
		gateway.sin_len = sizeof(gateway);
		gateway.sin_family = AF_INET;
		gateway.sin_addr.s_addr = imsg->gateway.s_addr;

		rtm.rtm_flags |= RTF_GATEWAY | RTF_STATIC;
		rtm.rtm_addrs |= RTA_GATEWAY;
		rtm.rtm_msglen += sizeof(gateway);

		iov[iovcnt].iov_base = &gateway;
		iov[iovcnt++].iov_len = sizeof(gateway);
	}

	/* Add netmask of 0. */
	memset(&mask, 0, sizeof(mask));

	mask.sin_len = sizeof(mask);
	mask.sin_family = AF_INET;

	rtm.rtm_addrs |= RTA_NETMASK;
	rtm.rtm_msglen += sizeof(mask);

	iov[iovcnt].iov_base = &mask;
	iov[iovcnt++].iov_len = sizeof(mask);

	/* Add our label so we can identify the route as our creation. */
	memset(&label, 0, sizeof(label));
	label.sr_len = sizeof(label);
	label.sr_family = AF_UNSPEC;
	len = snprintf(label.sr_label, sizeof(label.sr_label), "DHCLIENT %d",
	    getpid());
	if (len == -1)
		error("writing label for default route: %s", strerror(errno));
	if (len >= sizeof(label.sr_label))
		error("label for default route too long (%zd)",
		    sizeof(label.sr_label));

	rtm.rtm_addrs |= RTA_LABEL;
	rtm.rtm_msglen += sizeof(label);

	iov[iovcnt].iov_base = &label;
	iov[iovcnt++].iov_len = sizeof(label);

	/* Check for EEXIST since other dhclient may not be done. */
	for (i = 0; i < 5; i++) {
		if (writev(s, iov, iovcnt) != -1)
			break;
		if (errno != EEXIST && errno != ENETUNREACH)
			error("failed to add default route: %s",
			    strerror(errno));
		sleep(1);
	}

	close(s);
}

/*
 * Delete all existing inet addresses on interface.
 */
void
delete_addresses(char *ifname, int rdomain)
{
	struct in_addr addr;
	struct ifaddrs *ifap, *ifa;

	if (getifaddrs(&ifap) != 0)
		error("delete_addresses getifaddrs: %s", strerror(errno));

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if ((ifa->ifa_flags & IFF_LOOPBACK) ||
		    (ifa->ifa_flags & IFF_POINTOPOINT) ||
		    (!(ifa->ifa_flags & IFF_UP)) ||
		    (ifa->ifa_addr->sa_family != AF_INET) ||
		    (strcmp(ifi->name, ifa->ifa_name)))
			continue;

		memset(&addr, 0, sizeof(addr));
		memcpy(&addr,
		    &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr,
		    sizeof(in_addr_t));

		delete_address(ifi->name, ifi->rdomain, addr);
 	}

	freeifaddrs(ifap);
}

/*
 * [priv_]delete_address is the equivalent of
 *
 *	ifconfig <ifname> inet <addr> delete
 *	route -q <rdomain> delete <addr> 127.0.0.1
 */
void
delete_address(char *ifname, int rdomain, struct in_addr addr)
{
	struct imsg_delete_address	 imsg;
	int				 rslt;

	memset(&imsg, 0, sizeof(imsg));

	/* Note the address we are deleting for RTM_DELADDR filtering! */
	deleting.s_addr = addr.s_addr;

	strlcpy(imsg.ifname, ifname, sizeof(imsg.ifname));
	imsg.rdomain = rdomain;
	imsg.addr = addr;

	rslt = imsg_compose(unpriv_ibuf, IMSG_DELETE_ADDRESS, 0, 0 , -1, &imsg,
	    sizeof(imsg));

	if (rslt == -1)
		warning("delete_address: imsg_compose: %s", strerror(errno));
}

void
priv_delete_address(struct imsg_delete_address *imsg)
{
	struct ifaliasreq ifaliasreq;
	struct rt_msghdr rtm;
	struct sockaddr_in dest, gateway;
	struct iovec iov[3];
	struct sockaddr_in *in;
	int s, iovcnt = 0;

	/*
	 * Delete specified address on specified interface.
	 */

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		error("socket open failed: %s", strerror(errno));

	memset(&ifaliasreq, 0, sizeof(ifaliasreq));
	strncpy(ifaliasreq.ifra_name, imsg->ifname,
	    sizeof(ifaliasreq.ifra_name));

	in = (struct sockaddr_in *)&ifaliasreq.ifra_addr;
	in->sin_family = AF_INET;
	in->sin_len = sizeof(ifaliasreq.ifra_addr);
	in->sin_addr.s_addr = imsg->addr.s_addr;

	/* SIOCDIFADDR will result in a RTM_DELADDR message we must catch! */
	if (ioctl(s, SIOCDIFADDR, &ifaliasreq) == -1) {
		warning("SIOCDIFADDR failed (%s): %s", inet_ntoa(imsg->addr),
		    strerror(errno));
		close(s);
		return;
	}

	close(s);

	/*
	 * Delete the 127.0.0.1 route for the specified address.
	 */

	if ((s = socket(AF_ROUTE, SOCK_RAW, 0)) == -1)
		error("Routing Socket open failed: %s", strerror(errno));

	/* Build RTM header */

	memset(&rtm, 0, sizeof(rtm));

	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_type = RTM_DELETE;
	rtm.rtm_tableid = imsg->rdomain;
	rtm.rtm_priority = 0;
	rtm.rtm_msglen = sizeof(rtm);

	iov[iovcnt].iov_base = &rtm;
	iov[iovcnt++].iov_len = sizeof(rtm);
	
	/* Set destination address */

	memset(&dest, 0, sizeof(dest));

	dest.sin_len = sizeof(dest);
	dest.sin_family = AF_INET;
	dest.sin_addr.s_addr = imsg->addr.s_addr;

	rtm.rtm_addrs |= RTA_DST;
	rtm.rtm_msglen += sizeof(dest);

	iov[iovcnt].iov_base = &dest;
	iov[iovcnt++].iov_len = sizeof(dest);
	
	/* Set gateway address */

	memset(&gateway, 0, sizeof(gateway));

	gateway.sin_len = sizeof(gateway);
	gateway.sin_family = AF_INET;
	gateway.sin_addr.s_addr = inet_addr("127.0.0.1");

	rtm.rtm_flags |= RTF_GATEWAY;
	rtm.rtm_addrs |= RTA_GATEWAY;
	rtm.rtm_msglen += sizeof(gateway);

	iov[iovcnt].iov_base = &gateway;
	iov[iovcnt++].iov_len = sizeof(gateway);

	/* ESRCH means the route does not exist to delete. */
	if ((writev(s, iov, iovcnt) == -1) && (errno != ESRCH))
		error("failed to delete 127.0.0.1: %s", strerror(errno));

	close(s);
}

/*
 * [priv_]add_address is the equivalent of
 *
 *	ifconfig <if> inet <addr> netmask <mask> broadcast <addr>
 *	route -q <rdomain> add <addr> 127.0.0.1
 */
void
add_address(char *ifname, int rdomain, struct in_addr addr,
    struct in_addr mask)
{
	struct imsg_add_address imsg;
	int			rslt;
 
	memset(&imsg, 0, sizeof(imsg));

	/* Note the address we are adding for RTM_NEWADDR filtering! */
	adding = addr;

	strlcpy(imsg.ifname, ifname, sizeof(imsg.ifname));
	imsg.rdomain = rdomain;
	imsg.addr = addr;
	imsg.mask = mask;
 
	rslt = imsg_compose(unpriv_ibuf, IMSG_ADD_ADDRESS, 0, 0, -1, &imsg,
	    sizeof(imsg));

	if (rslt == -1)
		warning("add_address: imsg_compose: %s", strerror(errno));
}

void
priv_add_address(struct imsg_add_address *imsg)
{
	struct ifaliasreq ifaliasreq;
	struct rt_msghdr rtm;
	struct sockaddr_in dest, gateway;
	struct sockaddr_rtlabel label;
	struct iovec iov[4];
	struct sockaddr_in *in;
	int s, len, i, iovcnt = 0;

	if (imsg->addr.s_addr == INADDR_ANY) {
		/* Notification that the active_addr has been deleted. */
		active_addr.s_addr = INADDR_ANY;
		quit = INT_MAX;
		return;
	}

	/*
	 * Add specified address on specified interface.
	 */

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		error("socket open failed: %s", strerror(errno));

	memset(&ifaliasreq, 0, sizeof(ifaliasreq));
	strncpy(ifaliasreq.ifra_name, imsg->ifname,
	    sizeof(ifaliasreq.ifra_name));

	/* The actual address in ifra_addr. */
	in = (struct sockaddr_in *)&ifaliasreq.ifra_addr;
	in->sin_family = AF_INET;
	in->sin_len = sizeof(ifaliasreq.ifra_addr);
	in->sin_addr.s_addr = imsg->addr.s_addr;

	/* And the netmask in ifra_mask. */
	in = (struct sockaddr_in *)&ifaliasreq.ifra_mask;
	in->sin_family = AF_INET;
	in->sin_len = sizeof(ifaliasreq.ifra_mask);
	memcpy(&in->sin_addr.s_addr, &imsg->mask, sizeof(imsg->mask));

	/* No need to set broadcast address. Kernel can figure it out. */

	if (ioctl(s, SIOCAIFADDR, &ifaliasreq) == -1)
		warning("SIOCAIFADDR failed (%s): %s", inet_ntoa(imsg->addr),
		    strerror(errno));

	close(s);

	/*
	 * Add the 127.0.0.1 route for the specified address.
	 */

	if ((s = socket(AF_ROUTE, SOCK_RAW, 0)) == -1)
		error("Routing Socket open failed: %s", strerror(errno));

	/* Build RTM header */

	memset(&rtm, 0, sizeof(rtm));

	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_type = RTM_ADD;
	rtm.rtm_tableid = imsg->rdomain;
	rtm.rtm_priority = 0;
	rtm.rtm_msglen = sizeof(rtm);

	iov[iovcnt].iov_base = &rtm;
	iov[iovcnt++].iov_len = sizeof(rtm);
	
	/* Set destination address */

	memset(&dest, 0, sizeof(dest));

	dest.sin_len = sizeof(dest);
	dest.sin_family = AF_INET;
	dest.sin_addr.s_addr = imsg->addr.s_addr;

	rtm.rtm_addrs |= RTA_DST;
	rtm.rtm_msglen += sizeof(dest);

	iov[iovcnt].iov_base = &dest;
	iov[iovcnt++].iov_len = sizeof(dest);
	
	/* Set gateway address */

	memset(&gateway, 0, sizeof(gateway));

	gateway.sin_len = sizeof(gateway);
	gateway.sin_family = AF_INET;
	gateway.sin_addr.s_addr = inet_addr("127.0.0.1");

	rtm.rtm_flags |= RTF_GATEWAY;
	rtm.rtm_addrs |= RTA_GATEWAY;
	rtm.rtm_msglen += sizeof(gateway);

	iov[iovcnt].iov_base = &gateway;
	iov[iovcnt++].iov_len = sizeof(gateway);

	/* Add our label so we can identify the route as our creation. */
	memset(&label, 0, sizeof(label));

	label.sr_len = sizeof(label);
	label.sr_family = AF_UNSPEC;
	len = snprintf(label.sr_label, sizeof(label.sr_label), "DHCLIENT %d",
	    getpid());
	if (len == -1)
		error("writing label for host route: %s", strerror(errno));
	if (len >= sizeof(label.sr_label))
		error("label for host route too long (%zd)",
		    sizeof(label.sr_label));

	rtm.rtm_addrs |= RTA_LABEL;
	rtm.rtm_msglen += sizeof(label);

	iov[iovcnt].iov_base = &label;
	iov[iovcnt++].iov_len = sizeof(label);

	/* Check for EEXIST since other dhclient may not be done. */
	for (i = 0; i < 5; i++) {
		if (writev(s, iov, iovcnt) != -1)
			break;
		/* XXX Why do some systems get ENETUNREACH? */
		if (errno == ENETUNREACH) {
			note("failed to add 127.0.0.1 route: %s",
			    strerror(errno));
			break;
		} else if (errno != EEXIST)
			error("failed to add 127.0.0.1 route: %s",
			    strerror(errno));
		sleep(1);
	}

	close(s);

	active_addr = imsg->addr;
}

/*
 * [priv_]cleanup removes dhclient installed routes and address.
 */
void
cleanup(struct client_lease *active)
{
	struct imsg_cleanup imsg;
	int rslt;

	memset(&imsg, 0, sizeof(imsg));

	strlcpy(imsg.ifname, ifi->name, sizeof(imsg.ifname));
	imsg.rdomain = ifi->rdomain;
	if (active)
		imsg.addr = active->address;

	rslt = imsg_compose(unpriv_ibuf, IMSG_CLEANUP, 0, 0, -1,
	    &imsg, sizeof(imsg));
	if (rslt == -1)
		warning("cleanup: imsg_compose: %s", strerror(errno));

	/* Do flush so cleanup message gets through immediately. */
	rslt = imsg_flush(unpriv_ibuf);
	if (rslt == -1)
		warning("cleanup: imsg_flush: %s", strerror(errno));
}

void
priv_cleanup(struct imsg_cleanup *imsg)
{
	struct imsg_flush_routes fimsg;
	struct imsg_delete_address dimsg;

	memset(&fimsg, 0, sizeof(fimsg));
	fimsg.rdomain = imsg->rdomain;
	priv_flush_routes_and_arp_cache(&fimsg);

	if (imsg->addr.s_addr == INADDR_ANY)
		return;

	memset(&dimsg, 0, sizeof(dimsg));
	strlcpy(dimsg.ifname, imsg->ifname, sizeof(imsg->ifname));
	dimsg.rdomain = imsg->rdomain;
	dimsg.addr = imsg->addr;
	priv_delete_address(&dimsg);
}
