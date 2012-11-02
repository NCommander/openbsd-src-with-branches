/*	$OpenBSD: dispatch.c,v 1.60 2012/10/30 18:39:44 krw Exp $	*/

/*
 * Copyright 2004 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 1995, 1996, 1997, 1998, 1999
 * The Internet Software Consortium.   All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon <mellon@fugue.com> in cooperation with Vixie
 * Enterprises.  To learn more about the Internet Software Consortium,
 * see ``http://www.vix.com/isc''.  To learn more about Vixie
 * Enterprises, see ``http://www.vix.com''.
 */

#include "dhcpd.h"

#include <sys/ioctl.h>

#include <net/if_media.h>
#include <ifaddrs.h>
#include <poll.h>

struct dhcp_timeout timeout;

/*
 * Use getifaddrs() to get a list of all the attached interfaces.  Find
 * our interface on the list and store the interesting information about it.
 */
void
discover_interface(void)
{
	struct ifaddrs *ifap, *ifa;
	struct ifreq *tif;
	int len = IFNAMSIZ + sizeof(struct sockaddr_storage);

	if (getifaddrs(&ifap) != 0)
		error("getifaddrs failed");

	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if ((ifa->ifa_flags & IFF_LOOPBACK) ||
		    (ifa->ifa_flags & IFF_POINTOPOINT) ||
		    (!(ifa->ifa_flags & IFF_UP)))
			continue;

		if (strcmp(ifi->name, ifa->ifa_name))
			continue;

		/*
		 * If we have the capability, extract & save link information.
		 */
		if (ifa->ifa_addr->sa_family == AF_LINK) {
			struct sockaddr_dl *foo =
			    (struct sockaddr_dl *)ifa->ifa_addr;

			ifi->index = foo->sdl_index;
			ifi->hw_address.hlen = foo->sdl_alen;
			ifi->hw_address.htype = HTYPE_ETHER; /* XXX */
			memcpy(ifi->hw_address.haddr,
			    LLADDR(foo), foo->sdl_alen);
		}
		if (!ifi->ifp) {
			if ((tif = malloc(len)) == NULL)
				error("no space to remember ifp");
			strlcpy(tif->ifr_name, ifa->ifa_name, IFNAMSIZ);
			ifi->ifp = tif;
		}
	}

	if (!ifi->ifp)
		error("%s: not found", ifi->name);

	/* Register the interface... */
	if_register_receive();
	if_register_send();
	freeifaddrs(ifap);
}

/*
 * Loop waiting for packets, timeouts or routing messages.
 */
void
dispatch(void)
{
	int count, to_msec;
	struct pollfd fds[3];
	time_t cur_time, howlong;
	void (*func)(void);

	do {
		/*
		 * Call expired timeout, and then if there's still
		 * a timeout registered, time out the select call then.
		 */
another:
		if (!ifi)
			error("No interfaces available");

		if (ifi->rdomain != get_rdomain(ifi->name))
			error("Interface %s:"
			    " rdomain changed out from under us",
			    ifi->name);

		if (timeout.func) {
			time(&cur_time);
			if (timeout.when <= cur_time) {
				func = timeout.func;
				cancel_timeout();
				(*(func))();
				goto another;
			}
			/*
			 * Figure timeout in milliseconds, and check for
			 * potential overflow, so we can cram into an
			 * int for poll, while not polling with a
			 * negative timeout and blocking indefinitely.
			 */
			howlong = timeout.when - cur_time;
			if (howlong > INT_MAX / 1000)
				howlong = INT_MAX / 1000;
			to_msec = howlong * 1000;
		} else
			to_msec = -1;

		/* Set up the descriptors to be polled. */
		if (!ifi || ifi->rfdesc == -1)
			error("No live interface to poll on");

		fds[0].fd = ifi->rfdesc;
		fds[1].fd = routefd; /* Could be -1, which will be ignored. */
		fds[2].fd = privfd;
		fds[0].events = fds[1].events = fds[2].events = POLLIN;

		/* Wait for a packet or a timeout or privfd ... XXX */
		count = poll(fds, 3, to_msec);

		/* Not likely to be transitory... */
		if (count == -1) {
			if (errno == EAGAIN || errno == EINTR) {
				continue;
			} else
				error("poll: %m");
		}

		if ((fds[0].revents & (POLLIN | POLLHUP))) {
			if (ifi && ifi->linkstat && ifi->rfdesc != -1)
				got_one();
		}
		if ((fds[1].revents & (POLLIN | POLLHUP))) {
			if (ifi)
				routehandler();
		}
		if ((fds[2].revents & (POLLIN | POLLHUP))) {
			error("lost connection to [priv]");
		}
	} while (1);
}

void
got_one(void)
{
	struct sockaddr_in from;
	struct hardware hfrom;
	struct iaddr ifrom;
	ssize_t result;

	if ((result = receive_packet(&from, &hfrom)) == -1) {
		warning("receive_packet failed on %s: %s", ifi->name,
		    strerror(errno));
		ifi->errors++;
		if ((!interface_status(ifi->name)) ||
		    (ifi->noifmedia && ifi->errors > 20)) {
			/* our interface has gone away. */
			error("Interface %s no longer appears valid.",
			    ifi->name);
		}
		return;
	}
	if (result == 0)
		return;

	ifrom.len = 4;
	memcpy(ifrom.iabuf, &from.sin_addr, ifrom.len);

	do_packet(result, from.sin_port, ifrom, &hfrom);
}

void
interface_link_forceup(char *ifname)
{
	struct ifreq ifr;
	int sock;

	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		error("Can't create socket");

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (ioctl(sock, SIOCGIFFLAGS, (caddr_t)&ifr) == -1) {
		note("interface_link_forceup: SIOCGIFFLAGS failed (%m)");
		close(sock);
		return;
	}

	/* Force it down and up so others notice link state change. */
	ifr.ifr_flags &= !IFF_UP;
	if (ioctl(sock, SIOCSIFFLAGS, (caddr_t)&ifr) == -1) {
		note("interface_link_forceup: SIOCSIFFLAGS DOWN failed (%m)");
		close(sock);
		return;
	}

	ifr.ifr_flags |= IFF_UP;
	if (ioctl(sock, SIOCSIFFLAGS, (caddr_t)&ifr) == -1) {
		note("interface_link_forceup: SIOCSIFFLAGS UP failed (%m)");
		close(sock);
		return;
	}

	close(sock);
}

int
interface_status(char *ifname)
{
	struct ifreq ifr;
	struct ifmediareq ifmr;
	int sock;

	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		error("Can't create socket");

	/* get interface flags */
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (ioctl(sock, SIOCGIFFLAGS, &ifr) == -1) {
		error("ioctl(SIOCGIFFLAGS) on %s: %m", ifname);
	}

	/*
	 * if one of UP and RUNNING flags is dropped,
	 * the interface is not active.
	 */
	if ((ifr.ifr_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING))
		goto inactive;

	/* Next, check carrier on the interface, if possible */
	if (ifi->noifmedia)
		goto active;
	memset(&ifmr, 0, sizeof(ifmr));
	strlcpy(ifmr.ifm_name, ifname, sizeof(ifmr.ifm_name));
	if (ioctl(sock, SIOCGIFMEDIA, (caddr_t)&ifmr) == -1) {
		/*
		 * EINVAL or ENOTTY simply means that the interface
		 * does not support the SIOCGIFMEDIA ioctl. We regard it alive.
		 */
#ifdef DEBUG
		if (errno != EINVAL && errno != ENOTTY)
			debug("ioctl(SIOCGIFMEDIA) on %s: %m", ifname);
#endif

		ifi->noifmedia = 1;
		goto active;
	}
	if (ifmr.ifm_status & IFM_AVALID) {
		if (ifmr.ifm_status & IFM_ACTIVE)
			goto active;
		else
			goto inactive;
	}

	/* Assume 'active' if IFM_AVALID is not set. */

active:
	close(sock);
	return (1);
inactive:
	close(sock);
	return (0);
}

void
set_timeout(time_t when, void (*where)(void))
{
	timeout.when = when;
	timeout.func = where;
}

void
set_timeout_interval(time_t secs, void (*where)(void))
{
	timeout.when = time(NULL) + secs; 
	timeout.func = where;
}

void
cancel_timeout(void)
{
	timeout.when = 0;
	timeout.func = NULL;
}

int
get_rdomain(char *name)
{
	int rv = 0, s;
	struct ifreq ifr;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	    error("get_rdomain socket: %m");

	bzero(&ifr, sizeof(ifr));
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFRDOMAIN, (caddr_t)&ifr) != -1)
	    rv = ifr.ifr_rdomainid;

	close(s);
	return rv;
}

int
subnet_exists(struct client_lease *l)
 {
	struct ifaddrs *ifap, *ifa;
	in_addr_t mymask, myaddr, mynet, hismask, hisaddr, hisnet;
	int myrdomain, hisrdomain;

	bcopy(l->options[DHO_SUBNET_MASK].data, &mymask, 4);
	bcopy(l->address.iabuf, &myaddr, 4);
	mynet = mymask & myaddr;

	myrdomain = get_rdomain(ifi->name);

	if (getifaddrs(&ifap) != 0)
		error("getifaddrs failed");

	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if (strcmp(ifi->name, ifa->ifa_name) == 0)
			continue;

		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;

		hisrdomain = get_rdomain(ifi->name);
		if (hisrdomain != myrdomain)
			continue;

		hismask = ((struct sockaddr_in *)ifa->ifa_netmask)->
		    sin_addr.s_addr;
		hisaddr = ((struct sockaddr_in *)ifa->ifa_addr)->
		    sin_addr.s_addr;
		hisnet = hisaddr & hismask;

		if (hisnet == 0)
			continue;

		/* Would his packets go out *my* interface? */
		if (mynet == (hisaddr & mymask)) {
			note("interface %s already has the offered subnet!",
			    ifa->ifa_name);
			return (1);
		}
		
		/* Would my packets go out *his* interface? */
		if (hisnet == (myaddr & hismask)) {
			note("interface %s already has the offered subnet!",
			    ifa->ifa_name);
			return (1);
		}
	}

	freeifaddrs(ifap);

	return (0);
 }
