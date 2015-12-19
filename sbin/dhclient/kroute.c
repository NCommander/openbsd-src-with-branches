/*	$OpenBSD: kroute.c,v 1.77 2015/12/19 01:09:10 krw Exp $	*/

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
#include <signal.h>

struct in_addr active_addr;

int	create_route_label(struct sockaddr_rtlabel *);
int	check_route_label(struct sockaddr_rtlabel *);
void	populate_rti_info(struct sockaddr **, struct rt_msghdr *);
void	delete_route(int, struct rt_msghdr *);

#define	ROUTE_LABEL_NONE		1
#define	ROUTE_LABEL_NOT_DHCLIENT	2
#define	ROUTE_LABEL_DHCLIENT_OURS	3
#define	ROUTE_LABEL_DHCLIENT_UNKNOWN	4
#define	ROUTE_LABEL_DHCLIENT_LIVE	5
#define	ROUTE_LABEL_DHCLIENT_DEAD	6

/*
 * Do equivalent of
 *
 *	route -q $rdomain -n flush -inet -iface $interface
 *	arp -dan
 */
void
flush_routes(void)
{
	struct imsg_flush_routes imsg;
	int			 rslt;

	imsg.zapzombies = 1;

	rslt = imsg_compose(unpriv_ibuf, IMSG_FLUSH_ROUTES, 0, 0, -1,
	    &imsg, sizeof(imsg));
	if (rslt == -1)
		warning("flush_routes: imsg_compose: %s", strerror(errno));

	flush_unpriv_ibuf("flush_routes");
}

void
priv_flush_routes(struct imsg_flush_routes *imsg)
{
	char ifname[IF_NAMESIZE];
	struct sockaddr *rti_info[RTAX_MAX];
	int mib[7];
	size_t needed;
	char *lim, *buf = NULL, *bufp, *next, *errmsg = NULL;
	struct rt_msghdr *rtm;
	struct sockaddr_in *sa_in;
	struct sockaddr_rtlabel *sa_rl;
	int s;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_FLAGS;
	mib[5] = RTF_GATEWAY;
	mib[6] = ifi->rdomain;

	while (1) {
		if (sysctl(mib, 7, NULL, &needed, NULL, 0) == -1) {
			errmsg = "sysctl size of routes:";
			break;
		}
		if (needed == 0) {
			free(buf);
			return;
		}
		if ((bufp = realloc(buf, needed)) == NULL) {
			errmsg = "routes buf realloc:";
			break;
		}
		buf = bufp;
		if (sysctl(mib, 7, buf, &needed, NULL, 0) == -1) {
			if (errno == ENOMEM)
				continue;
			errmsg = "sysctl retrieval of routes:";
			break;
		}
		break;
	}

	if (errmsg) {
		warning("route cleanup failed - %s %s (msize=%zu)",
		    errmsg, strerror(errno), needed);
		free(buf);
		return;
	}

	if ((s = socket(AF_ROUTE, SOCK_RAW, 0)) == -1)
		error("opening socket to flush routes: %s", strerror(errno));

	lim = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		if (rtm->rtm_version != RTM_VERSION)
			continue;

		populate_rti_info(rti_info, rtm);

		sa_rl = (struct sockaddr_rtlabel *)rti_info[RTAX_LABEL];
		sa_in = (struct sockaddr_in *)rti_info[RTAX_NETMASK];

		switch (check_route_label(sa_rl)) {
		case ROUTE_LABEL_DHCLIENT_OURS:
			/* Always delete routes we labeled. */
			delete_route(s, rtm);
			break;
		case ROUTE_LABEL_DHCLIENT_DEAD:
			if (imsg->zapzombies)
				delete_route(s, rtm);
			break;
		case ROUTE_LABEL_DHCLIENT_LIVE:
		case ROUTE_LABEL_DHCLIENT_UNKNOWN:
			/* Another dhclient's responsibility. */
			break;
		case ROUTE_LABEL_NONE:
		case ROUTE_LABEL_NOT_DHCLIENT:
			/* Delete default routes on our interface. */
			if (if_indextoname(rtm->rtm_index, ifname) &&
			    sa_in &&
			    sa_in->sin_addr.s_addr == INADDR_ANY &&
			    rtm->rtm_tableid == ifi->rdomain &&
			    strcmp(ifi->name, ifname) == 0)
				delete_route(s, rtm);
			break;
		default:
			break;
		}
	}

	close(s);
	free(buf);
}

void
add_route(struct in_addr dest, struct in_addr netmask,
    struct in_addr gateway, struct in_addr ifa, int addrs, int flags)
{
	struct imsg_add_route	 imsg;
	int			 rslt;

	imsg.dest = dest;
	imsg.gateway = gateway;
	imsg.netmask = netmask;
	imsg.ifa = ifa;
	imsg.addrs = addrs;
	imsg.flags = flags;

	rslt = imsg_compose(unpriv_ibuf, IMSG_ADD_ROUTE, 0, 0, -1,
	    &imsg, sizeof(imsg));
	if (rslt == -1)
		warning("add_route: imsg_compose: %s", strerror(errno));

	flush_unpriv_ibuf("add_route");
}

void
priv_add_route(struct imsg_add_route *imsg)
{
	char destbuf[INET_ADDRSTRLEN], gatewaybuf[INET_ADDRSTRLEN];
	char maskbuf[INET_ADDRSTRLEN], ifabuf[INET_ADDRSTRLEN];
	struct rt_msghdr rtm;
	struct sockaddr_in dest, gateway, mask, ifa;
	struct sockaddr_rtlabel label;
	struct iovec iov[6];
	int s, i, iovcnt = 0;

	if ((s = socket(AF_ROUTE, SOCK_RAW, 0)) == -1)
		error("Routing Socket open failed: %s", strerror(errno));

	memset(destbuf, 0, sizeof(destbuf));
	memset(maskbuf, 0, sizeof(maskbuf));
	memset(gatewaybuf, 0, sizeof(gatewaybuf));
	memset(ifabuf, 0, sizeof(ifabuf));

	/* Build RTM header */

	memset(&rtm, 0, sizeof(rtm));

	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_type = RTM_ADD;
	rtm.rtm_tableid = ifi->rdomain;
	rtm.rtm_priority = RTP_NONE;
	rtm.rtm_msglen = sizeof(rtm);
	rtm.rtm_addrs = imsg->addrs;
	rtm.rtm_flags = imsg->flags;

	iov[iovcnt].iov_base = &rtm;
	iov[iovcnt++].iov_len = sizeof(rtm);

	if (imsg->addrs & RTA_DST) {
		strlcpy(destbuf, inet_ntoa(imsg->dest), sizeof(destbuf));
		memset(&dest, 0, sizeof(dest));

		dest.sin_len = sizeof(dest);
		dest.sin_family = AF_INET;
		dest.sin_addr.s_addr = imsg->dest.s_addr;

		rtm.rtm_msglen += sizeof(dest);

		iov[iovcnt].iov_base = &dest;
		iov[iovcnt++].iov_len = sizeof(dest);
	}

	if (imsg->addrs & RTA_GATEWAY) {
		strlcpy(gatewaybuf, inet_ntoa(imsg->gateway),
		    sizeof(gatewaybuf));
		memset(&gateway, 0, sizeof(gateway));

		gateway.sin_len = sizeof(gateway);
		gateway.sin_family = AF_INET;
		gateway.sin_addr.s_addr = imsg->gateway.s_addr;

		rtm.rtm_msglen += sizeof(gateway);

		iov[iovcnt].iov_base = &gateway;
		iov[iovcnt++].iov_len = sizeof(gateway);
	}

	if (imsg->addrs & RTA_NETMASK) {
		strlcpy(maskbuf, inet_ntoa(imsg->netmask), sizeof(maskbuf));
		memset(&mask, 0, sizeof(mask));

		mask.sin_len = sizeof(mask);
		mask.sin_family = AF_INET;
		mask.sin_addr.s_addr = imsg->netmask.s_addr;

		rtm.rtm_msglen += sizeof(mask);

		iov[iovcnt].iov_base = &mask;
		iov[iovcnt++].iov_len = sizeof(mask);
	}

	if (imsg->addrs & RTA_IFA) {
		strlcpy(ifabuf, inet_ntoa(imsg->ifa), sizeof(ifabuf));
		memset(&ifa, 0, sizeof(ifa));

		ifa.sin_len = sizeof(ifa);
		ifa.sin_family = AF_INET;
		ifa.sin_addr.s_addr = imsg->ifa.s_addr;

		rtm.rtm_msglen += sizeof(ifa);

		iov[iovcnt].iov_base = &ifa;
		iov[iovcnt++].iov_len = sizeof(ifa);
	}

	/* Add our label so we can identify the route as our creation. */
	if (create_route_label(&label) == 0) {
		rtm.rtm_addrs |= RTA_LABEL;
		rtm.rtm_msglen += sizeof(label);
		iov[iovcnt].iov_base = &label;
		iov[iovcnt++].iov_len = sizeof(label);
	}

	/* Check for EEXIST since other dhclient may not be done. */
	for (i = 0; i < 5; i++) {
		if (writev(s, iov, iovcnt) != -1)
			break;
		if (i == 4)
			warning("failed to add route (%s/%s via %s/%s): %s",
			    destbuf, maskbuf, gatewaybuf, ifabuf,
			    strerror(errno));
		else if (errno == EEXIST || errno == ENETUNREACH)
			sleep(1);
	}

	close(s);
}

/*
 * Delete all existing inet addresses on interface.
 */
void
delete_addresses(void)
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

		memcpy(&addr, &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr,
		    sizeof(addr));

		delete_address(addr);
	}

	freeifaddrs(ifap);
}

/*
 * [priv_]delete_address is the equivalent of
 *
 *	ifconfig <ifname> inet <addr> delete
 */
void
delete_address(struct in_addr addr)
{
	struct imsg_delete_address	 imsg;
	int				 rslt;

	/* Note the address we are deleting for RTM_DELADDR filtering! */
	deleting.s_addr = addr.s_addr;

	imsg.addr = addr;

	rslt = imsg_compose(unpriv_ibuf, IMSG_DELETE_ADDRESS, 0, 0 , -1, &imsg,
	    sizeof(imsg));
	if (rslt == -1)
		warning("delete_address: imsg_compose: %s", strerror(errno));

	flush_unpriv_ibuf("delete_address");
}

void
priv_delete_address(struct imsg_delete_address *imsg)
{
	struct ifaliasreq ifaliasreq;
	struct sockaddr_in *in;
	int s;

	/*
	 * Delete specified address on specified interface.
	 */

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		error("socket open failed: %s", strerror(errno));

	memset(&ifaliasreq, 0, sizeof(ifaliasreq));
	strncpy(ifaliasreq.ifra_name, ifi->name, sizeof(ifaliasreq.ifra_name));

	in = (struct sockaddr_in *)&ifaliasreq.ifra_addr;
	in->sin_family = AF_INET;
	in->sin_len = sizeof(ifaliasreq.ifra_addr);
	in->sin_addr.s_addr = imsg->addr.s_addr;

	/* SIOCDIFADDR will result in a RTM_DELADDR message we must catch! */
	if (ioctl(s, SIOCDIFADDR, &ifaliasreq) == -1) {
		if (errno != EADDRNOTAVAIL)
			warning("SIOCDIFADDR failed (%s): %s",
			    inet_ntoa(imsg->addr), strerror(errno));
	}

	close(s);
}

/*
 * [priv_]set_interface_mtu is the equivalent of
 *
 *      ifconfig <if> mtu <mtu>
 */
void
set_interface_mtu(int mtu)
{
	struct imsg_set_interface_mtu imsg;
	int rslt;

	imsg.mtu = mtu;

	rslt = imsg_compose(unpriv_ibuf, IMSG_SET_INTERFACE_MTU, 0, 0, -1,
	    &imsg, sizeof(imsg));
	if (rslt == -1)
		warning("set_interface_mtu: imsg_compose: %s", strerror(errno));

	flush_unpriv_ibuf("set_interface_mtu");
}

void
priv_set_interface_mtu(struct imsg_set_interface_mtu *imsg)
{
	struct ifreq ifr;
	int s;

	memset(&ifr, 0, sizeof(ifr));

	strlcpy(ifr.ifr_name, ifi->name, sizeof(ifr.ifr_name));
	ifr.ifr_mtu = imsg->mtu;

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		error("socket open failed: %s", strerror(errno));
	if (ioctl(s, SIOCSIFMTU, &ifr) == -1)
		warning("SIOCSIFMTU failed (%d): %s", imsg->mtu,
		    strerror(errno));
	close(s);
}

/*
 * [priv_]add_address is the equivalent of
 *
 *	ifconfig <if> inet <addr> netmask <mask> broadcast <addr>
 */
void
add_address(struct in_addr addr, struct in_addr mask)
{
	struct imsg_add_address imsg;
	int			rslt;

	/* Note the address we are adding for RTM_NEWADDR filtering! */
	adding = addr;

	imsg.addr = addr;
	imsg.mask = mask;

	rslt = imsg_compose(unpriv_ibuf, IMSG_ADD_ADDRESS, 0, 0, -1, &imsg,
	    sizeof(imsg));
	if (rslt == -1)
		warning("add_address: imsg_compose: %s", strerror(errno));

	flush_unpriv_ibuf("add_address");
}

void
priv_add_address(struct imsg_add_address *imsg)
{
	struct ifaliasreq ifaliasreq;
	struct sockaddr_in *in;
	int s;

	if (imsg->addr.s_addr == INADDR_ANY) {
		/* Notification that the active_addr has been deleted. */
		active_addr.s_addr = INADDR_ANY;
		quit = INTERNALSIG;
		return;
	}

	/*
	 * Add specified address on specified interface.
	 */

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		error("socket open failed: %s", strerror(errno));

	memset(&ifaliasreq, 0, sizeof(ifaliasreq));
	strncpy(ifaliasreq.ifra_name, ifi->name, sizeof(ifaliasreq.ifra_name));

	/* The actual address in ifra_addr. */
	in = (struct sockaddr_in *)&ifaliasreq.ifra_addr;
	in->sin_family = AF_INET;
	in->sin_len = sizeof(ifaliasreq.ifra_addr);
	in->sin_addr.s_addr = imsg->addr.s_addr;

	/* And the netmask in ifra_mask. */
	in = (struct sockaddr_in *)&ifaliasreq.ifra_mask;
	in->sin_family = AF_INET;
	in->sin_len = sizeof(ifaliasreq.ifra_mask);
	memcpy(&in->sin_addr, &imsg->mask, sizeof(in->sin_addr));

	/* No need to set broadcast address. Kernel can figure it out. */

	if (ioctl(s, SIOCAIFADDR, &ifaliasreq) == -1)
		warning("SIOCAIFADDR failed (%s): %s", inet_ntoa(imsg->addr),
		    strerror(errno));

	close(s);

	active_addr = imsg->addr;
}

/*
 * Inform the [priv] process a HUP was received and it should restart.
 */
void
sendhup(struct client_lease *active)
{
	struct imsg_hup imsg;
	int rslt;

	if (active)
		imsg.addr = active->address;
	else
		imsg.addr.s_addr = INADDR_ANY;

	rslt = imsg_compose(unpriv_ibuf, IMSG_HUP, 0, 0, -1,
	    &imsg, sizeof(imsg));
	if (rslt == -1)
		warning("sendhup: imsg_compose: %s", strerror(errno));

	flush_unpriv_ibuf("sendhup");
}

/*
 * priv_cleanup removes dhclient installed routes and address.
 */
void
priv_cleanup(struct imsg_hup *imsg)
{
	struct imsg_flush_routes fimsg;
	struct imsg_delete_address dimsg;

	fimsg.zapzombies = 0;	/* Only zapzombies when binding a lease. */
	priv_flush_routes(&fimsg);

	if (imsg->addr.s_addr == INADDR_ANY)
		return;

	dimsg.addr = imsg->addr;
	priv_delete_address(&dimsg);
}

int
resolv_conf_priority(void)
{
	struct iovec iov[3];
	struct {
		struct rt_msghdr	m_rtm;
		char			m_space[512];
	} m_rtmsg;
	struct sockaddr *rti_info[RTAX_MAX];
	struct sockaddr_in sin;
	struct sockaddr_rtlabel *sa_rl;
	pid_t pid;
	ssize_t len;
	u_int32_t seq;
	int s, rslt, iovcnt = 0;

	rslt = 0;

	s = socket(PF_ROUTE, SOCK_RAW, AF_INET);
	if (s == -1) {
		warning("default route socket: %s", strerror(errno));
		return (0);
	}

	/* Build RTM header */

	memset(&m_rtmsg, 0, sizeof(m_rtmsg));

	m_rtmsg.m_rtm.rtm_version = RTM_VERSION;
	m_rtmsg.m_rtm.rtm_type = RTM_GET;
	m_rtmsg.m_rtm.rtm_msglen = sizeof(m_rtmsg.m_rtm);
	m_rtmsg.m_rtm.rtm_flags = RTF_STATIC | RTF_GATEWAY | RTF_UP;
	m_rtmsg.m_rtm.rtm_seq = seq = arc4random();
	m_rtmsg.m_rtm.rtm_tableid = ifi->rdomain;

	iov[iovcnt].iov_base = &m_rtmsg.m_rtm;
	iov[iovcnt++].iov_len = sizeof(m_rtmsg.m_rtm);

	/* Set destination & netmask addresses of all zeros. */

	m_rtmsg.m_rtm.rtm_addrs = RTA_DST | RTA_NETMASK;

	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;

	iov[iovcnt].iov_base = &sin;
	iov[iovcnt++].iov_len = sizeof(sin);
	iov[iovcnt].iov_base = &sin;
	iov[iovcnt++].iov_len = sizeof(sin);

	m_rtmsg.m_rtm.rtm_msglen += 2 * sizeof(sin);

	if (writev(s, iov, iovcnt) == -1) {
		if (errno != ESRCH)
			warning("RTM_GET of default route: %s",
			    strerror(errno));
		goto done;
	}

	pid = getpid();

	do {
		len = read(s, &m_rtmsg, sizeof(m_rtmsg));
		if (len == -1) {
			warning("get default route read: %s", strerror(errno));
			break;
		} else if (len == 0) {
			warning("no data from default route read");
			break;
		}
		if (m_rtmsg.m_rtm.rtm_version != RTM_VERSION)
			continue;
		if (m_rtmsg.m_rtm.rtm_type == RTM_GET &&
		    m_rtmsg.m_rtm.rtm_pid == pid &&
		    m_rtmsg.m_rtm.rtm_seq == seq) {
			if (m_rtmsg.m_rtm.rtm_errno) {
				warning("default route read rtm: %s",
				    strerror(m_rtmsg.m_rtm.rtm_errno));
				goto done;
			}
			break;
		}
	} while (1);

	populate_rti_info(rti_info, &m_rtmsg.m_rtm);

	sa_rl = (struct sockaddr_rtlabel *)rti_info[RTAX_LABEL];
	if (check_route_label(sa_rl) == ROUTE_LABEL_DHCLIENT_OURS)
		rslt = 1;

done:
	close(s);
	return (rslt);
}

int
create_route_label(struct sockaddr_rtlabel *label)
{
	int len;

	memset(label, 0, sizeof(*label));

	label->sr_len = sizeof(label);
	label->sr_family = AF_UNSPEC;

	len = snprintf(label->sr_label, sizeof(label->sr_label), "DHCLIENT %d",
	    (int)getpid());

	if (len == -1) {
		warning("creating route label: %s", strerror(errno));
		return (1);
	}

	if (len >= sizeof(label->sr_label)) {
		warning("creating route label: label too long (%d vs %zu)", len,
		    sizeof(label->sr_label));
		return (1);
	}

	return (0);
}

int
check_route_label(struct sockaddr_rtlabel *label)
{
	pid_t pid;

	if (!label)
		return (ROUTE_LABEL_NONE);

	if (strncmp("DHCLIENT ", label->sr_label, 9))
		return (ROUTE_LABEL_NOT_DHCLIENT);

	pid = (pid_t)strtonum(label->sr_label + 9, 1, INT_MAX, NULL);
	if (pid <= 0)
		return (ROUTE_LABEL_DHCLIENT_UNKNOWN);

	if (pid == getpid())
		return (ROUTE_LABEL_DHCLIENT_OURS);

	if (kill(pid, 0) == -1) {
		if (errno == ESRCH)
			return (ROUTE_LABEL_DHCLIENT_DEAD);
		else
			return (ROUTE_LABEL_DHCLIENT_UNKNOWN);
	}

	return (ROUTE_LABEL_DHCLIENT_LIVE);
}

void
populate_rti_info(struct sockaddr **rti_info, struct rt_msghdr *rtm)
{
	struct sockaddr *sa;
	int i;

#define ROUNDUP(a) \
    ((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

	sa = (struct sockaddr *)((char *)(rtm) + rtm->rtm_hdrlen);

	for (i = 0; i < RTAX_MAX; i++) {
		if (rtm->rtm_addrs & (1 << i)) {
			rti_info[i] = sa;
			sa = (struct sockaddr *)((char *)(sa) +
			    ROUNDUP(sa->sa_len));
		} else
			rti_info[i] = NULL;
	}
}

void
delete_route(int s, struct rt_msghdr *rtm)
{
	static int seqno;
	ssize_t rlen;

	rtm->rtm_type = RTM_DELETE;
	rtm->rtm_tableid = ifi->rdomain;
	rtm->rtm_seq = seqno++;

	rlen = write(s, (char *)rtm, rtm->rtm_msglen);
	if (rlen == -1) {
		if (errno != ESRCH)
			error("RTM_DELETE write: %s", strerror(errno));
	} else if (rlen < (int)rtm->rtm_msglen)
		error("short RTM_DELETE write (%zd)\n", rlen);
}

void
flush_unpriv_ibuf(const char *who)
{
	while (unpriv_ibuf->w.queued) {
		if (msgbuf_write(&unpriv_ibuf->w) <= 0) {
			if (errno == EAGAIN)
				break;
			if (quit == 0)
				quit = INTERNALSIG;
			if (errno != EPIPE && errno != 0)
				warning("%s: msgbuf_write: %s", who,
				    strerror(errno));
			break;
		}
	}
}
