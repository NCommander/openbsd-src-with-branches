/*	$OpenBSD: dispatch.c,v 1.15 2004/02/24 17:26:43 henning Exp $	*/

/* Network input dispatcher... */

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

/* Most boxes has less than 16 interfaces, so this might be a good guess.  */
#define INITIAL_IFREQ_COUNT 16

struct interface_info *interfaces, *dummy_interfaces, *fallback_interface;
struct protocol *protocols;
struct timeout *timeouts;
static struct timeout *free_timeouts;
static int interfaces_invalidated;
void (*bootp_packet_handler)(struct interface_info *,
     struct dhcp_packet *, int, unsigned int,
     struct iaddr, struct hardware *);

static int interface_status(struct interface_info *ifinfo);

int quiet_interface_discovery;

/*
 * Use getifaddrs() to get a list of all the attached interfaces.  For
 * each interface that's of type INET and not the loopback interface,
 * register that interface with the network I/O software, figure out
 * what subnet it's on, and add it to the list of interfaces.
 */
void
discover_interfaces(void)
{
	struct interface_info *tmp;
	struct interface_info *last, *next;
	struct sockaddr_in foo;
	struct ifreq *tif;
	struct ifaddrs *ifap, *ifa;

	if (getifaddrs(&ifap) != 0)
		error("getifaddrs failed");

	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if ((ifa->ifa_flags & IFF_LOOPBACK) ||
		    (ifa->ifa_flags & IFF_POINTOPOINT) ||
		    (!(ifa->ifa_flags & IFF_UP)))
			continue;

		for (tmp = interfaces; tmp; tmp = tmp->next)
			if (!strcmp(tmp->name, ifa->ifa_name))
				break;

		if (!tmp)
			break;

		/*
		 * If we have the capability, extract link information
		 * and record it in a linked list.
		 */
		if (ifa->ifa_addr->sa_family == AF_LINK) {
			struct sockaddr_dl *foo =
			    (struct sockaddr_dl *)ifa->ifa_addr;
			tmp->index = foo->sdl_index;
			tmp->hw_address.hlen = foo->sdl_alen;
			tmp->hw_address.htype = HTYPE_ETHER; /* XXX */
			memcpy(tmp->hw_address.haddr,
			    LLADDR(foo), foo->sdl_alen);
		} else if (ifa->ifa_addr->sa_family == AF_INET) {
			struct iaddr addr;

			bcopy(ifa->ifa_addr, &foo, sizeof(foo));
			if (foo.sin_addr.s_addr == htonl(INADDR_LOOPBACK))
				continue;
			if (!tmp->ifp) {
				int len = IFNAMSIZ + ifa->ifa_addr->sa_len;
				tif = malloc(len);
				if (!tif)
					error("no space to remember ifp.");
				strlcpy(tif->ifr_name, ifa->ifa_name, IFNAMSIZ);
				memcpy(&tif->ifr_addr, ifa->ifa_addr,
				    ifa->ifa_addr->sa_len);
				tmp->ifp = tif;
				tmp->primary_address = foo.sin_addr;
			}
			addr.len = 4;
			memcpy(addr.iabuf, &foo.sin_addr.s_addr, addr.len);
		}
	}

	/*
	 * Now cycle through all the interfaces we found, looking for
	 * hardware addresses.
	 */

	/* Weed out the interfaces that did not have IP addresses. */
	last = NULL;
	for (tmp = interfaces; tmp; tmp = next) {
		next = tmp->next;
		if (!tmp->ifp || !(tmp->flags & INTERFACE_REQUESTED)) {
			if ((tmp->flags & INTERFACE_REQUESTED))
				error("%s: not found", tmp->name);
			if (!last)
				interfaces = interfaces->next;
			else
				last->next = tmp->next;

			/*
			 * Remember the interface in case we need to know
			 * about it later.
			 */
			tmp->next = dummy_interfaces;
			dummy_interfaces = tmp;
			continue;
		}
		last = tmp;

		memcpy(&foo, &tmp->ifp->ifr_addr, sizeof(tmp->ifp->ifr_addr));

		/* Register the interface... */
		if_register_receive(tmp);
		if_register_send(tmp);
	}

	/* Now register all the remaining interfaces as protocols. */
	for (tmp = interfaces; tmp; tmp = tmp->next)
		add_protocol(tmp->name, tmp->rfdesc, got_one, tmp);

	freeifaddrs(ifap);
}

void
reinitialize_interfaces(void)
{
	interfaces_invalidated = 1;
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
	struct protocol *l;
	int nfds = 0;
	struct pollfd *fds;
	int count;
	int i;
	time_t howlong;
	int to_msec;

	nfds = 0;
	for (l = protocols; l; l = l->next)
		nfds++;

	fds = malloc(nfds * sizeof(struct pollfd));
	if (fds == NULL)
		error("Can't allocate poll structures.");

	do {
		/*
		 * Call any expired timeouts, and then if there's still
		 * a timeout registered, time out the select call then.
		 */
another:
		if (timeouts) {
			struct timeout *t;
			if (timeouts->when <= cur_time) {
				t = timeouts;
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
			 * negative timeout and blocking indefinetely.
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
			struct interface_info *ip;
			ip = l->local;
			if ((fds[i].revents & POLLIN)) {
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

	if ((result =
	    receive_packet(ip, u.packbuf, sizeof(u), &from, &hfrom)) == -1) {
		warn("receive_packet failed on %s: %s", ip->name,
		    strerror(errno));
		ip->errors++;
		if ((!interface_status(ip)) ||
		    (ip->noifmedia && ip->errors > 20)) {
			/* our interface has gone away. */
			warn("Interface %s no longer appears valid.",
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
	if (ioctl(ifsock, SIOCGIFFLAGS, &ifr) < 0) {
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
	if (ioctl(ifsock, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0) {
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

void
add_timeout(time_t when, void (*where)(void *), void *what)
{
	struct timeout *t, *q;

	/* See if this timeout supersedes an existing timeout. */
	t = NULL;
	for (q = timeouts; q; q = q->next) {
		if (q->func == where && q->what == what) {
			if (t)
				t->next = q->next;
			else
				timeouts = q->next;
			break;
		}
		t = q;
	}

	/* If we didn't supersede a timeout, allocate a timeout
	   structure now. */
	if (!q) {
		if (free_timeouts) {
			q = free_timeouts;
			free_timeouts = q->next;
			q->func = where;
			q->what = what;
		} else {
			q = malloc(sizeof(struct timeout));
			if (!q)
				error("Can't allocate timeout structure!");
			q->func = where;
			q->what = what;
		}
	}

	q->when = when;

	/* Now sort this timeout into the timeout list. */

	/* Beginning of list? */
	if (!timeouts || timeouts->when > q->when) {
		q->next = timeouts;
		timeouts = q;
		return;
	}

	/* Middle of list? */
	for (t = timeouts; t->next; t = t->next) {
		if (t->next->when > q->when) {
			q->next = t->next;
			t->next = q;
			return;
		}
	}

	/* End of list. */
	t->next = q;
	q->next = NULL;
}

void
cancel_timeout(void (*where)(void *), void *what)
{
	struct timeout *t, *q;

	/* Look for this timeout on the list, and unlink it if we find it. */
	t = NULL;
	for (q = timeouts; q; q = q->next) {
		if (q->func == where && q->what == what) {
			if (t)
				t->next = q->next;
			else
				timeouts = q->next;
			break;
		}
		t = q;
	}

	/* If we found the timeout, put it on the free list. */
	if (q) {
		q->next = free_timeouts;
		free_timeouts = q;
	}
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

int
interface_link_status(char *ifname)
{
	int sock;
	struct ifmediareq ifmr;

	if ((sock = socket (AF_INET, SOCK_DGRAM, 0)) == -1)
		error("Can't create socket");

	memset(&ifmr, 0, sizeof(ifmr));
	strlcpy(ifmr.ifm_name, ifname, sizeof(ifmr.ifm_name));
	if (ioctl(sock, SIOCGIFMEDIA, (caddr_t)&ifmr) == -1) {
		/* EINVAL -> link state unknown. treat as active */
		if (errno != EINVAL)
			syslog(LOG_DEBUG, "ioctl(SIOCGIFMEDIA) on %s: %m",
			    ifname);
		close(sock);
		return (1);
	}
	close(sock);

	if (ifmr.ifm_status & IFM_AVALID)
		if ((ifmr.ifm_active & IFM_NMASK) == IFM_ETHER) {
			if (ifmr.ifm_status & IFM_ACTIVE)
				return (1);
			else
				return (0);
		}

	return (1);
}
