/*	$OpenBSD: dispatch.c,v 1.11 2016/08/27 01:26:22 guenther Exp $	*/

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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in.h>

#include <errno.h>
#include <ifaddrs.h>
#include <limits.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "dhcp.h"
#include "dhcpd.h"

struct protocol *protocols;
struct timeout *timeouts;
static struct timeout *free_timeouts;
static int interfaces_invalidated;

void (*bootp_packet_handler)(struct interface_info *,
    struct dhcp_packet *, int, unsigned int,
    struct iaddr, struct hardware *);

static int interface_status(struct interface_info *ifinfo);

struct interface_info *
get_interface(const char *ifname, void (*handler)(struct protocol *))
{
	struct interface_info		*iface;
	struct ifaddrs			*ifap, *ifa;
	struct ifreq			*tif;
	struct sockaddr_in		 foo;

	if ((iface = calloc(1, sizeof(*iface))) == NULL)
		error("failed to allocate memory");

	if (strlcpy(iface->name, ifname, sizeof(iface->name)) >=
	    sizeof(iface->name))
		error("interface name too long");

	if (getifaddrs(&ifap) != 0)
		error("getifaddrs failed");

	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if ((ifa->ifa_flags & IFF_LOOPBACK) ||
		    (ifa->ifa_flags & IFF_POINTOPOINT) ||
		    (!(ifa->ifa_flags & IFF_UP)))
			continue;

		if (strcmp(ifname, ifa->ifa_name))
			continue;

		/*
		 * If we have the capability, extract link information
		 * and record it in a linked list.
		 */
		if (ifa->ifa_addr->sa_family == AF_LINK) {
			struct sockaddr_dl *foo =
			    (struct sockaddr_dl *)ifa->ifa_addr;
			struct if_data *ifi =
			    (struct if_data *)ifa->ifa_data;

			iface->index = foo->sdl_index;
			iface->hw_address.hlen = foo->sdl_alen;
			if (ifi->ifi_type == IFT_ENC)
				iface->hw_address.htype = HTYPE_IPSEC_TUNNEL;
			else
				iface->hw_address.htype = HTYPE_ETHER; /* XXX */
			memcpy(iface->hw_address.haddr,
			    LLADDR(foo), foo->sdl_alen);
		} else if (ifa->ifa_addr->sa_family == AF_INET) {
			struct iaddr addr;

			memcpy(&foo, ifa->ifa_addr, sizeof(foo));
			if (foo.sin_addr.s_addr == htonl(INADDR_LOOPBACK))
				continue;
			if (!iface->ifp) {
				int len = IFNAMSIZ + ifa->ifa_addr->sa_len;

				if ((tif = malloc(len)) == NULL)
					error("no space to remember ifp");
				strlcpy(tif->ifr_name, ifa->ifa_name, IFNAMSIZ);
				memcpy(&tif->ifr_addr, ifa->ifa_addr,
				    ifa->ifa_addr->sa_len);
				iface->ifp = tif;
				iface->primary_address = foo.sin_addr;
			}
			addr.len = 4;
			memcpy(addr.iabuf, &foo.sin_addr.s_addr, addr.len);
		}
	}

	freeifaddrs(ifap);

	if (!iface->ifp)
		error("%s: not found", iface->name);

	/* Register the interface... */
	if_register_receive(iface);
	if_register_send(iface);
	add_protocol(iface->name, iface->rfdesc, handler, iface);

	return (iface);
}

/*
 * Wait for packets to come in using poll().  When a packet comes in,
 * call receive_packet to receive the packet and possibly strip hardware
 * addressing information from it, and then call through the
 * bootp_packet_handler hook to try to do something with it.
 */
void
dispatch(void)
{
	int count, i, to_msec, nfds = 0;
	struct protocol *l;
	struct pollfd *fds;
	time_t howlong;

	nfds = 0;
	for (l = protocols; l; l = l->next)
		nfds++;

	fds = calloc(nfds, sizeof(struct pollfd));
	if (fds == NULL)
		error("Can't allocate poll structures.");

	do {
		/*
		 * Call any expired timeouts, and then if there's still
		 * a timeout registered, time out the select call then.
		 */
another:
		if (timeouts) {
			if (timeouts->when <= cur_time) {
				struct timeout *t = timeouts;

				timeouts = timeouts->next;
				(*(t->func))(t->what);
				t->next = free_timeouts;
				free_timeouts = t;
				goto another;
			}

			/*
			 * Figure timeout in milliseconds, and check for
			 * potential overflow, so we can cram into an
			 * int for poll, while not polling with a
			 * negative timeout and blocking indefinitely.
			 */
			howlong = timeouts->when - cur_time;
			if (howlong > INT_MAX / 1000)
				howlong = INT_MAX / 1000;
			to_msec = howlong * 1000;
		} else
			to_msec = -1;

		/* Set up the descriptors to be polled. */
		i = 0;

		for (l = protocols; l; l = l->next) {
			struct interface_info *ip = l->local;

			if (ip && (l->handler != got_one || !ip->dead)) {
				fds[i].fd = l->fd;
				fds[i].events = POLLIN;
				fds[i].revents = 0;
				i++;
			}
		}

		if (i == 0)
			error("No live interfaces to poll on - exiting.");

		/* Wait for a packet or a timeout... XXX */
		count = poll(fds, nfds, to_msec);

		/* Not likely to be transitory... */
		if (count == -1) {
			if (errno == EAGAIN || errno == EINTR) {
				time(&cur_time);
				continue;
			}
			else
				error("poll: %m");
		}

		/* Get the current time... */
		time(&cur_time);

		i = 0;
		for (l = protocols; l; l = l->next) {
			struct interface_info *ip = l->local;

			if ((fds[i].revents & (POLLIN | POLLHUP))) {
				fds[i].revents = 0;
				if (ip && (l->handler != got_one ||
				    !ip->dead))
					(*(l->handler))(l);
				if (interfaces_invalidated)
					break;
			}
			i++;
		}
		interfaces_invalidated = 0;
	} while (1);
}


void
got_one(struct protocol *l)
{
	struct sockaddr_in from;
	struct hardware hfrom;
	struct iaddr ifrom;
	size_t result;
	union {
		/*
		 * Packet input buffer.  Must be as large as largest
		 * possible MTU.
		 */
		unsigned char packbuf[4095];
		struct dhcp_packet packet;
	} u;
	struct interface_info *ip = l->local;

	if ((result = receive_packet(ip, u.packbuf, sizeof(u), &from,
	    &hfrom)) == -1) {
		warning("receive_packet failed on %s: %s", ip->name,
		    strerror(errno));
		ip->errors++;
		if ((!interface_status(ip)) ||
		    (ip->noifmedia && ip->errors > 20)) {
			/* our interface has gone away. */
			warning("Interface %s no longer appears valid.",
			    ip->name);
			ip->dead = 1;
			interfaces_invalidated = 1;
			close(l->fd);
			remove_protocol(l);
			free(ip);
		}
		return;
	}
	if (result == 0)
		return;

	if (bootp_packet_handler) {
		ifrom.len = 4;
		memcpy(ifrom.iabuf, &from.sin_addr, ifrom.len);

		(*bootp_packet_handler)(ip, &u.packet, result,
		    from.sin_port, ifrom, &hfrom);
	}
}

int
interface_status(struct interface_info *ifinfo)
{
	char *ifname = ifinfo->name;
	int ifsock = ifinfo->rfdesc;
	struct ifreq ifr;
	struct ifmediareq ifmr;

	/* get interface flags */
	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (ioctl(ifsock, SIOCGIFFLAGS, &ifr) == -1) {
		syslog(LOG_ERR, "ioctl(SIOCGIFFLAGS) on %s: %m", ifname);
		goto inactive;
	}
	/*
	 * if one of UP and RUNNING flags is dropped,
	 * the interface is not active.
	 */
	if ((ifr.ifr_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) {
		goto inactive;
	}
	/* Next, check carrier on the interface, if possible */
	if (ifinfo->noifmedia)
		goto active;
	memset(&ifmr, 0, sizeof(ifmr));
	strlcpy(ifmr.ifm_name, ifname, sizeof(ifmr.ifm_name));
	if (ioctl(ifsock, SIOCGIFMEDIA, (caddr_t)&ifmr) == -1) {
		if (errno != EINVAL) {
			syslog(LOG_DEBUG, "ioctl(SIOCGIFMEDIA) on %s: %m",
			    ifname);

			ifinfo->noifmedia = 1;
			goto active;
		}
		/*
		 * EINVAL (or ENOTTY) simply means that the interface
		 * does not support the SIOCGIFMEDIA ioctl. We regard it alive.
		 */
		ifinfo->noifmedia = 1;
		goto active;
	}
	if (ifmr.ifm_status & IFM_AVALID) {
		switch (ifmr.ifm_active & IFM_NMASK) {
		case IFM_ETHER:
			if (ifmr.ifm_status & IFM_ACTIVE)
				goto active;
			else
				goto inactive;
			break;
		default:
			goto inactive;
		}
	}
inactive:
	return (0);
active:
	return (1);
}

/* Add a protocol to the list of protocols... */
void
add_protocol(char *name, int fd, void (*handler)(struct protocol *),
    void *local)
{
	struct protocol *p;

	p = malloc(sizeof(*p));
	if (!p)
		error("can't allocate protocol struct for %s", name);

	p->fd = fd;
	p->handler = handler;
	p->local = local;
	p->next = protocols;
	protocols = p;
}

void
remove_protocol(struct protocol *proto)
{
	struct protocol *p, *next, *prev;

	prev = NULL;
	for (p = protocols; p; p = next) {
		next = p->next;
		if (p == proto) {
			if (prev)
				prev->next = p->next;
			else
				protocols = p->next;
			free(p);
		}
	}
}
