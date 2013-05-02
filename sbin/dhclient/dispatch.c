/*	$OpenBSD: dispatch.c,v 1.77 2013/04/05 19:19:05 krw Exp $	*/

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
#include "privsep.h"

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
	struct option_data *opt;
	char *data;
	int len;

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
			memcpy(ifi->hw_address.haddr, LLADDR(foo),
			    foo->sdl_alen);
			opt = &config->send_options[DHO_DHCP_CLIENT_IDENTIFIER];
			if (opt->len == 0) {
				/* Build default client identifier. */
				data = calloc(1, foo->sdl_alen + 1);
				if (data != NULL) {
					data[0] = ifi->hw_address.htype;
					memcpy(&data[1], LLADDR(foo),
					    foo->sdl_alen);
					opt->data = data;
					opt->len = foo->sdl_alen + 1;
				}
			}
		}

		if (!ifi->ifp) {
			len = IFNAMSIZ + sizeof(struct sockaddr_storage);
			if ((tif = malloc(len)) == NULL)
				error("no space to remember ifp");
			strlcpy(tif->ifr_name, ifa->ifa_name, IFNAMSIZ);
			ifi->ifp = tif;
		}
	}

	if (!ifi->ifp)
		error("%s: not found", ifi->name);

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

	while (quit == 0) {
		/*
		 * Call expired timeout, and then if there's still
		 * a timeout registered, time out the select call then.
		 */
another:
		if (!ifi) {
			warning("No interfaces available");
			quit = INTERNALSIG;
			continue;
		}

		if (ifi->rdomain != get_rdomain(ifi->name)) {
			warning("Interface %s:"
			    " rdomain changed out from under us",
			    ifi->name);
			quit = INTERNALSIG;
			continue;
		}

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
		if (!ifi || ifi->rfdesc == -1) {
			warning("No live interface to poll on");
			quit = INTERNALSIG;
			continue;
		}

		fds[0].fd = ifi->rfdesc;
		fds[1].fd = routefd; /* Could be -1, which will be ignored. */
		fds[2].fd = unpriv_ibuf->fd;
		fds[0].events = fds[1].events = fds[2].events = POLLIN;

		if (unpriv_ibuf->w.queued)
			fds[2].events |= POLLOUT;

		/* Wait for a packet or a timeout or unpriv_ibuf->fd. XXX */
		count = poll(fds, 3, to_msec);

		/* Not likely to be transitory. */
		if (count == -1) {
			if (errno == EAGAIN || errno == EINTR) {
				continue;
			} else {
				warning("poll: %s", strerror(errno));
				quit = INTERNALSIG;
				continue;
			}
		}

		if ((fds[0].revents & (POLLIN | POLLHUP))) {
			if (ifi && ifi->linkstat && ifi->rfdesc != -1)
				got_one();
		}
		if ((fds[1].revents & (POLLIN | POLLHUP))) {
			if (ifi)
				routehandler();
		}
		if (fds[2].revents & POLLOUT) {
			if (msgbuf_write(&unpriv_ibuf->w) == -1) {
				warning("pipe write error to [priv]");
				quit = INTERNALSIG;
				continue;
			}
		}
		if ((fds[2].revents & (POLLIN | POLLHUP))) {
			/* Pipe to [priv] closed. Assume it emitted error. */
			quit = INTERNALSIG;
			continue;
		}
	}

	if (quit == SIGHUP) {
		/* Tell [priv] process that HUP has occurred. */
		sendhup(client->active);
		warning("%s; restarting", strsignal(quit));
		exit (0);
	} else if (quit != INTERNALSIG) {
		warning("%s; exiting", strsignal(quit));
		exit(1);
	}
}

void
got_one(void)
{
	struct sockaddr_in from;
	struct hardware hfrom;
	struct in_addr ifrom;
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

	memcpy(&ifrom, &from.sin_addr, sizeof(ifrom));

	do_packet(from.sin_port, ifrom, &hfrom);
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
		note("interface_link_forceup: SIOCGIFFLAGS failed (%s)",
		    strerror(errno));
		close(sock);
		return;
	}

	/* Force it down and up so others notice link state change. */
	ifr.ifr_flags &= ~IFF_UP;
	if (ioctl(sock, SIOCSIFFLAGS, (caddr_t)&ifr) == -1) {
		note("interface_link_forceup: SIOCSIFFLAGS DOWN failed (%s)",
		    strerror(errno));
		close(sock);
		return;
	}

	ifr.ifr_flags |= IFF_UP;
	if (ioctl(sock, SIOCSIFFLAGS, (caddr_t)&ifr) == -1) {
		note("interface_link_forceup: SIOCSIFFLAGS UP failed (%s)",
		    strerror(errno));
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

	/* Get interface flags. */
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (ioctl(sock, SIOCGIFFLAGS, &ifr) == -1) {
		error("ioctl(SIOCGIFFLAGS) on %s: %s", ifname,
		    strerror(errno));
	}

	if ((ifr.ifr_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING))
		goto inactive;

	/* Next, check carrier on the interface if possible. */
	if (ifi->noifmedia)
		goto active;
	memset(&ifmr, 0, sizeof(ifmr));
	strlcpy(ifmr.ifm_name, ifname, sizeof(ifmr.ifm_name));
	if (ioctl(sock, SIOCGIFMEDIA, (caddr_t)&ifmr) == -1) {
		/*
		 * EINVAL or ENOTTY simply means that the interface does not
		 * support the SIOCGIFMEDIA ioctl. We regard it alive.
		 */
#ifdef DEBUG
		if (errno != EINVAL && errno != ENOTTY)
			debug("ioctl(SIOCGIFMEDIA) on %s: %s", ifname,
			    strerror(errno));
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
	    error("get_rdomain socket: %s", strerror(errno));

	memset(&ifr, 0, sizeof(ifr));
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
	struct in_addr mymask, myaddr, mynet, hismask, hisaddr, hisnet;
	int myrdomain, hisrdomain;

	memset(&mymask, 0, sizeof(mymask));
	memcpy(&mymask.s_addr, l->options[DHO_SUBNET_MASK].data,
	    l->options[DHO_SUBNET_MASK].len);
	myaddr.s_addr = l->address.s_addr;
	mynet.s_addr = mymask.s_addr & myaddr.s_addr;

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

		memcpy(&hismask,
		    &((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr,
		    sizeof(hismask));
		memcpy(&hisaddr,
		    &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr,
		    sizeof(hisaddr));
		hisnet.s_addr = hisaddr.s_addr & hismask.s_addr;

		if (hisnet.s_addr == INADDR_ANY)
			continue;

		/* Would his packets go out *my* interface? */
		if (mynet.s_addr == (hisaddr.s_addr & mymask.s_addr)) {
			note("interface %s already has the offered subnet!",
			    ifa->ifa_name);
			return (1);
		}
		
		/* Would my packets go out *his* interface? */
		if (hisnet.s_addr == (myaddr.s_addr & hismask.s_addr)) {
			note("interface %s already has the offered subnet!",
			    ifa->ifa_name);
			return (1);
		}
	}

	freeifaddrs(ifap);

	return (0);
 }
