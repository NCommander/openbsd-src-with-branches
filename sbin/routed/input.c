/*	$NetBSD: input.c,v 1.16 1995/07/13 23:20:10 christos Exp $	*/

/*
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)input.c	8.1 (Berkeley) 6/5/93";
#else
static char rcsid[] = "$NetBSD: input.c,v 1.16 1995/07/13 23:20:10 christos Exp $";
#endif
#endif /* not lint */

/*
 * Routing Table Management Daemon
 */
#include "defs.h"
#include <syslog.h>


/*
 * "Authenticate" router from which message originated.
 * We accept routing packets from routers directly connected
 * via broadcast or point-to-point networks,
 * and from those listed in /etc/gateways.
 */
static struct interface *
rip_verify(from)
	struct sockaddr *from;
{
	struct interface *ifp;
	char buf[256];

	if ((ifp = if_iflookup(from)) == 0) {
		syslog(LOG_ERR, "trace command from unknown router, %s",
		       (*afswitch[from->sa_family].af_format)(from, buf,
							      sizeof(buf)));
		return NULL;
	}

	if ((ifp->int_flags & 
		(IFF_BROADCAST|IFF_POINTOPOINT|IFF_REMOTE)) == 0) {
		syslog(LOG_ERR,
		       "trace command from router %s, with bad flags %x",
		       (*afswitch[from->sa_family].af_format)(from, buf,
							      sizeof(buf)),
		       ifp->int_flags);
		return NULL;
	}

	if ((ifp->int_flags & IFF_PASSIVE) != 0) {
		syslog(LOG_ERR,
		       "trace command from %s on an active interface",
		       (*afswitch[from->sa_family].af_format)(from, buf,
							      sizeof(buf)));
		return NULL;
	}

	return ifp;
}


/*
 * Process a newly received packet.
 */
void
rip_input(from, rip, size)
	struct sockaddr *from;
	register struct rip *rip;
	int size;
{
	register struct rt_entry *rt;
	register struct netinfo *n;
	register struct interface *ifp;
	struct sockaddr dst, gateway, netmask;
	int count, changes = 0;
	register struct afswitch *afp;
	static struct sockaddr badfrom;
	char buf1[256], buf2[256];

	ifp = 0;
	TRACE_INPUT(ifp, from, (char *)rip, size);
	if (from->sa_family >= af_max ||
	    (afp = &afswitch[from->sa_family])->af_hash == NULL) {
		syslog(LOG_INFO,
	 "\"from\" address in unsupported address family (%d), cmd %d\n",
		    from->sa_family, rip->rip_cmd);
		return;
	}
	if (rip->rip_vers == 0) {
		syslog(LOG_ERR,
		    "RIP version 0 packet received from %s! (cmd %d)",
		    (*afswitch[from->sa_family].af_format)(from, buf1,
							   sizeof(buf1)),
		    rip->rip_cmd);
		return;
	}

	switch (rip->rip_cmd) {

	case RIPCMD_REQUEST:
		n = rip->rip_nets;
		count = size - ((char *)n - (char *)rip);
		if (count < sizeof (struct netinfo))
			return;
		for (; count > 0; n++) {
			if (count < sizeof (struct netinfo))
				break;
			count -= sizeof (struct netinfo);

			n->rip_metric = ntohl(n->rip_metric);
			n->rip_family = ntohs(n->rip_family);
			/* 
			 * A single entry with sa_family == AF_UNSPEC and
			 * metric ``infinity'' means ``all routes''.
			 * We respond to routers only if we are acting
			 * as a supplier, or to anyone other than a router
			 * (eg, query).
			 */
			if (n->rip_family == AF_UNSPEC &&
			    n->rip_metric == HOPCNT_INFINITY && count == 0) {
			    	if (supplier || (*afp->af_portmatch)(from) == 0)
					supply(from, 0, 0, 0);
				return;
			}
			if (n->rip_family < af_max &&
			    afswitch[n->rip_family].af_hash) {
				if (!(*afswitch[n->rip_family].af_get)(
					DESTINATION, n, &dst))
					return;
				rt = rtlookup(&dst);
			}
			else
				rt = 0;
#define min(a, b) (a < b ? a : b)
			n->rip_metric = rt == 0 ? HOPCNT_INFINITY :
				min(rt->rt_metric + 1, HOPCNT_INFINITY);
			n->rip_metric = htonl(n->rip_metric);
		}
		rip->rip_cmd = RIPCMD_RESPONSE;
		memcpy(packet, rip, size);
		(*afp->af_output)(s, 0, from, size);
		return;

	case RIPCMD_TRACEON:
	case RIPCMD_TRACEOFF:
		/* verify message came from a privileged port */
#ifdef TRACING
		if ((*afp->af_portcheck)(from) == 0)
			return;

		if ((ifp = rip_verify(from)) == NULL)
			return;

		((char *)rip)[size] = '\0';
		if (rip->rip_cmd == RIPCMD_TRACEON)
			traceon(rip->rip_tracefile);
		else
			traceoff();
#endif
		return;

	case RIPCMD_RESPONSE:
		/* verify message came from a router */
		if ((*afp->af_portmatch)(from) == 0)
			return;
		(*afp->af_canon)(from);
		/* are we talking to ourselves? */
		ifp = if_ifwithaddr(from);
		if (ifp) {
			if (ifp->int_flags & IFF_PASSIVE) {
				syslog(LOG_ERR,
				  "bogus input (from passive interface, %s)",
				  (*afswitch[from->sa_family].af_format)(from,
							 buf1, sizeof(buf1)));
				return;
			}
			rt = rtfind(from);
			if (rt == 0 || (((rt->rt_state & RTS_INTERFACE) == 0) &&
			    rt->rt_metric >= ifp->int_metric))
				addrouteforif(ifp);
			else
				rt->rt_timer = 0;
			return;
		}
		/*
		 * Update timer for interface on which the packet arrived.
		 * If from other end of a point-to-point link that isn't
		 * in the routing tables, (re-)add the route.
		 */
		if ((rt = rtfind(from)) &&
		    (rt->rt_state & (RTS_INTERFACE | RTS_REMOTE)))
			rt->rt_timer = 0;
		else if ((ifp = if_ifwithdstaddr(from)) &&
		    (rt == 0 || rt->rt_metric >= ifp->int_metric))
			addrouteforif(ifp);

		if ((ifp = rip_verify(from)) == NULL)
			return;

		size -= 4 * sizeof (char);
		n = rip->rip_nets;
		for (; size > 0; size -= sizeof (struct netinfo), n++) {
			if (size < sizeof (struct netinfo))
				break;
			n->rip_metric = ntohl(n->rip_metric);
			n->rip_family = ntohs(n->rip_family);
			if (!(*afswitch[n->rip_family].af_get)(DESTINATION, n,
							       &dst))
				continue;
			if (!(*afswitch[n->rip_family].af_get)(NETMASK,
							       n, &netmask))
				memset(&netmask, 0, sizeof(netmask));
			if (!(*afswitch[n->rip_family].af_get)(GATEWAY,
							       n, &gateway))
				memcpy(&gateway, from, sizeof(gateway));
			if (dst.sa_family >= af_max ||
			    (afp = &afswitch[dst.sa_family])->af_hash == NULL) {
				syslog(LOG_INFO,
		"route in unsupported address family (%d), from %s (af %d)\n",
				   dst.sa_family,
				   (*afswitch[from->sa_family].af_format)(from,
							  buf1, sizeof(buf1)),
				   from->sa_family);
				continue;
			}
			if (((*afp->af_checkhost)(&dst)) == 0) {
				syslog(LOG_DEBUG,
				   "bad host %s in route from %s (af %d)\n",
				   (*afswitch[dst.sa_family].af_format)(
					&dst, buf1, sizeof(buf1)),
				   (*afswitch[from->sa_family].af_format)(from,
					buf2, sizeof(buf2)),
				   from->sa_family);
				continue;
			}
			if (n->rip_metric == 0 ||
			    (unsigned) n->rip_metric > HOPCNT_INFINITY) {
				if (memcmp(from, &badfrom,
					   sizeof(badfrom)) != 0) {
					syslog(LOG_ERR,
					    "bad metric (%d) from %s\n",
					    n->rip_metric,
				  (*afswitch[from->sa_family].af_format)(from,
						buf1, sizeof(buf1)));
					badfrom = *from;
				}
				continue;
			}
			/*
			 * Adjust metric according to incoming interface.
			 */
			if ((unsigned) n->rip_metric < HOPCNT_INFINITY)
				n->rip_metric += ifp->int_metric;
			if ((unsigned) n->rip_metric > HOPCNT_INFINITY)
				n->rip_metric = HOPCNT_INFINITY;
			rt = rtlookup(&dst);
			if (rt == 0 ||
			    (rt->rt_state & (RTS_INTERNAL|RTS_INTERFACE)) ==
			    (RTS_INTERNAL|RTS_INTERFACE)) {
				/*
				 * If we're hearing a logical network route
				 * back from a peer to which we sent it,
				 * ignore it.
				 */
				if (rt && rt->rt_state & RTS_SUBNET &&
				    (*afp->af_sendroute)(rt, from))
					continue;
				if ((unsigned)n->rip_metric < HOPCNT_INFINITY) {
				    /*
				     * Look for an equivalent route that
				     * includes this one before adding
				     * this route.
				     */
				    rt = rtfind(&dst);
				    if (rt && equal(&gateway, &rt->rt_router))
					    continue;
				    rtadd(&dst, &gateway, &netmask,
					  n->rip_metric, 0);
				    changes++;
				}
				continue;
			}

			/*
			 * Update if from gateway and different,
			 * shorter, or equivalent but old route
			 * is getting stale.
			 */
			if (equal(&gateway, &rt->rt_router)) {
				if (n->rip_metric != rt->rt_metric) {
					rtchange(rt, &gateway,
						 &netmask, n->rip_metric);
					changes++;
					rt->rt_timer = 0;
					if (rt->rt_metric >= HOPCNT_INFINITY)
						rt->rt_timer =
						    GARBAGE_TIME - EXPIRE_TIME;
				} else if (rt->rt_metric < HOPCNT_INFINITY)
					rt->rt_timer = 0;
			} else if ((unsigned) n->rip_metric < rt->rt_metric ||
			    (rt->rt_metric == n->rip_metric &&
			    rt->rt_timer > (EXPIRE_TIME/2) &&
			    (unsigned) n->rip_metric < HOPCNT_INFINITY)) {
				rtchange(rt, &gateway, &netmask, n->rip_metric);
				changes++;
				rt->rt_timer = 0;
			}
		}
		break;
	}

	/*
	 * If changes have occurred, and if we have not sent a broadcast
	 * recently, send a dynamic update.  This update is sent only
	 * on interfaces other than the one on which we received notice
	 * of the change.  If we are within MIN_WAITTIME of a full update,
	 * don't bother sending; if we just sent a dynamic update
	 * and set a timer (nextbcast), delay until that time.
	 * If we just sent a full update, delay the dynamic update.
	 * Set a timer for a randomized value to suppress additional
	 * dynamic updates until it expires; if we delayed sending
	 * the current changes, set needupdate.
	 */
	if (changes && supplier &&
	   now.tv_sec - lastfullupdate.tv_sec < SUPPLY_INTERVAL-MAX_WAITTIME) {
		u_long delay;

		if (now.tv_sec - lastbcast.tv_sec >= MIN_WAITTIME &&
		    timercmp(&nextbcast, &now, <)) {
			if (traceactions)
				fprintf(ftrace, "send dynamic update\n");
			toall(supply, RTS_CHANGED, ifp);
			lastbcast = now;
			needupdate = 0;
			nextbcast.tv_sec = 0;
		} else {
			needupdate++;
			if (traceactions)
				fprintf(ftrace, "delay dynamic update\n");
		}
#define RANDOMDELAY()	(MIN_WAITTIME * 1000000 + \
		(u_long)random() % ((MAX_WAITTIME - MIN_WAITTIME) * 1000000))

		if (nextbcast.tv_sec == 0) {
			delay = RANDOMDELAY();
			if (traceactions)
				fprintf(ftrace,
				    "inhibit dynamic update for %d usec\n",
				    delay);
			nextbcast.tv_sec = delay / 1000000;
			nextbcast.tv_usec = delay % 1000000;
			timeradd(&nextbcast, &now, &nextbcast);
			/*
			 * If the next possibly dynamic update
			 * is within MIN_WAITTIME of the next full update,
			 * force the delay past the full update,
			 * or we might send a dynamic update just before
			 * the full update.
			 */
			if (nextbcast.tv_sec > lastfullupdate.tv_sec +
			    SUPPLY_INTERVAL - MIN_WAITTIME)
				nextbcast.tv_sec = lastfullupdate.tv_sec +
				    SUPPLY_INTERVAL + 1;
		}
	}
}
