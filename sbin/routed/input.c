/*	$OpenBSD: input.c,v 1.6 1996/09/06 13:22:07 deraadt Exp $	*/

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

#if !defined(lint)
static char sccsid[] = "@(#)input.c	8.1 (Berkeley) 6/5/93";
#else
static char rcsid[] = "$OpenBSD: input.c,v 1.6 1996/09/06 13:22:07 deraadt Exp $";
#endif

#include "defs.h"

static void input(struct sockaddr_in *, struct interface*, struct rip *, int);
static void input_route(struct interface *, naddr,
			naddr, naddr, naddr, struct netinfo *);


/* process RIP input
 */
void
read_rip(int sock,
	 struct interface *ifp)
{
	struct sockaddr_in from;
	int fromlen, cc;
	union pkt_buf inbuf;


	for (;;) {
		fromlen = sizeof(from);
		cc = recvfrom(sock, &inbuf, sizeof(inbuf), 0,
			      (struct sockaddr*)&from, &fromlen);
		if (cc <= 0) {
			if (cc < 0 && errno != EWOULDBLOCK)
				LOGERR("recvfrom(rip)");
			break;
		}
		if (fromlen != sizeof(struct sockaddr_in))
			logbad(1,"impossible recvfrom(rip) fromlen=%d",
			       fromlen);

		input(&from, ifp, &inbuf.rip, cc);
	}
}


/* Process a RIP packet
 */
static void
input(struct sockaddr_in *from,		/* received from this IP address */
      struct interface *sifp,		/* interface by which it arrived */
      struct rip *rip,
      int size)
{
#	define FROM_NADDR from->sin_addr.s_addr
	static naddr use_auth, bad_len, bad_mask;
	static naddr unk_router, bad_router, bad_nhop;

	struct interface *aifp;		/* interface if via 1 hop */
	struct rt_entry *rt;
	struct netinfo *n, *lim;
	struct interface *ifp1;
	naddr gate, mask, v1_mask, dst, ddst_h;
	int i;

	aifp = iflookup(from->sin_addr.s_addr);
	if (sifp == 0)
		sifp = aifp;

	if (sifp != 0)
		sifp->int_state |= IS_ACTIVE;

	trace_rip("Recv", "from", from, sifp, rip, size);

	if (rip->rip_vers == 0) {
		if (from->sin_addr.s_addr != bad_router)
			msglog("RIP version 0, cmd %d, packet received"
			       " from %s",
			       rip->rip_cmd, naddr_ntoa(FROM_NADDR));
		bad_router = from->sin_addr.s_addr;
		return;
	} else if (rip->rip_vers > RIPv2) {
		rip->rip_vers = RIPv2;
	}
	if (size > MAXPACKETSIZE) {
		if (from->sin_addr.s_addr != bad_router)
			msglog("packet at least %d bytes too long received"
			       " from %s",
			       size-MAXPACKETSIZE, naddr_ntoa(FROM_NADDR));
		bad_router = from->sin_addr.s_addr;
		return;
	}

	n = rip->rip_nets;
	lim = (struct netinfo *)((char*)rip + size);

	/* Notice authentication.
	 * As required by section 4.2 in RFC 1723, discard authenticated
	 * RIPv2 messages, but only if configured for that silliness.
	 *
	 * RIPv2 authentication is lame, since snooping on the wire makes
	 * its simple passwords evident.  Also, why authenticate queries?
	 * Why should a RIPv2 implementation with authentication disabled
	 * not be able to listen to RIPv2 packets with authenication, while
	 * RIPv1 systems will listen?  Crazy!
	 */
	if (!auth_ok
	    && rip->rip_vers == RIPv2
	    && n < lim && n->n_family == RIP_AF_AUTH) {
		if (from->sin_addr.s_addr != use_auth)
			msglog("RIPv2 message with authentication"
			       " from %s discarded",
			       naddr_ntoa(FROM_NADDR));
		use_auth = from->sin_addr.s_addr;
		trace_pkt("discard authenticated RIPv2 message\n");
		return;
	}

	switch (rip->rip_cmd) {
	case RIPCMD_REQUEST:
		/* did the request come from a router?
		 */
		if (from->sin_port == htons(RIP_PORT)) {
			/* yes, ignore it if RIP is off so that it does not
			 * depend on us.
			 */
			if (rip_sock < 0) {
				trace_pkt("ignore request while RIP off\n");
				return;
			}

			/* Ignore the request if we talking to ourself
			 * (and not a remote gateway).
			 */
			if (ifwithaddr(FROM_NADDR, 0, 0) != 0) {
				trace_pkt("discard our own RIP request\n");
				return;
			}
		}

		/* According to RFC 1723, we should ignore unathenticated
		 * queries.  That is too silly to bother with.  Sheesh!
		 * Are forwarding tables supposed to be secret?  When
		 * a bad guy can infer them with test traffic?
		 * Maybe on firewalls you'd care, but not enough to
		 * give up the diagnostic facilities of remote probing.
		 */

		if (n >= lim
		    || size%sizeof(*n) != sizeof(struct rip)%sizeof(*n)) {
			if (from->sin_addr.s_addr != bad_len)
				msglog("request of bad length (%d) from %s",
				       size, naddr_ntoa(FROM_NADDR));
			bad_len = from->sin_addr.s_addr;
		}
		for (; n < lim; n++) {
			n->n_metric = ntohl(n->n_metric);

			/* A single entry with family RIP_AF_UNSPEC and
			 * metric HOPCNT_INFINITY means "all routes".
			 * We respond to routers only if we are acting
			 * as a supplier, or to anyone other than a router
			 * (i.e. a query).
			 */
			if (n->n_family == RIP_AF_UNSPEC
			    && n->n_metric == HOPCNT_INFINITY
			    && n == rip->rip_nets
			    && n+1 == lim) {
				if (from->sin_port != htons(RIP_PORT)) {
					/* Answer a query from a utility
					 * program with all we know.
					 */
					supply(from, sifp, OUT_QUERY, 0,
					       rip->rip_vers);
					return;
				}
				/* A router trying to prime its tables.
				 * Filter the answer in the about same way
				 * broadcasts are filtered.
				 *
				 * Only answer a router if we are a supplier
				 * to keep an unwary host that is just starting
				 * from picking us as a router.  Respond with
				 * RIPv1 instead of RIPv2 if that is what we
				 * are broadcasting on the interface to keep
				 * the remote router from getting the wrong
				 * initial idea of the routes we send.
				 */
				if (!supplier
				    || aifp == 0
				    || (aifp->int_state & IS_PASSIVE)
				    || (aifp->int_state & IS_ALIAS)
				    || ((aifp->int_state & IS_NO_RIPV1_OUT)
					&& (aifp->int_state&IS_NO_RIPV2_OUT)))
					return;

				supply(from, aifp, OUT_UNICAST, 0,
				       (aifp->int_state&IS_NO_RIPV1_OUT)
				       ? RIPv2 : RIPv1);
				return;
			}

			if (n->n_family != RIP_AF_INET) {
				if (from->sin_addr.s_addr != bad_router)
					msglog("request from %s"
					       " for unsupported (af %d) %s",
					       naddr_ntoa(FROM_NADDR),
					       ntohs(n->n_family),
					       naddr_ntoa(n->n_dst));
				bad_router = from->sin_addr.s_addr;
				return;
			}

			dst = n->n_dst;
			if (!check_dst(dst)) {
				if (from->sin_addr.s_addr != bad_router)
					msglog("bad queried destination"
					       " %s from %s",
					       naddr_ntoa(dst),
					       naddr_ntoa(FROM_NADDR));
				bad_router = from->sin_addr.s_addr;
				return;
			}

			if (rip->rip_vers == RIPv1
			    || 0 == (mask = ntohl(n->n_mask))
			    || 0 != (ntohl(dst) & ~mask))
				mask = ripv1_mask_host(dst,sifp);

			rt = rtget(dst, mask);
			if (!rt && dst != RIP_DEFAULT)
				rt = rtfind(n->n_dst);

			n->n_tag = 0;
			n->n_nhop = 0;
			if (rip->rip_vers == RIPv1) {
				n->n_mask = 0;
			} else {
				n->n_mask = mask;
			}
			if (rt == 0) {
				n->n_metric = HOPCNT_INFINITY;
			} else {
				n->n_metric = rt->rt_metric+1;
				n->n_metric += (sifp!=0)?sifp->int_metric : 1;
				if (n->n_metric > HOPCNT_INFINITY)
					n->n_metric = HOPCNT_INFINITY;
				if (rip->rip_vers != RIPv1) {
					n->n_tag = rt->rt_tag;
					if (sifp != 0
					    && on_net(rt->rt_gate,
						      sifp->int_net,
						      sifp->int_mask)
					    && rt->rt_gate != sifp->int_addr)
						n->n_nhop = rt->rt_gate;
				}
			}
			HTONL(n->n_metric);
		}
		/* Answer about specific routes.
		 * Only answer a router if we are a supplier
		 * to keep an unwary host that is just starting
		 * from picking us an a router.
		 */
		rip->rip_cmd = RIPCMD_RESPONSE;
		rip->rip_res1 = 0;
		if (rip->rip_vers != RIPv1)
			rip->rip_vers = RIPv2;
		if (from->sin_port != htons(RIP_PORT)) {
			/* query */
			(void)output(OUT_QUERY, from, sifp, rip, size);
		} else if (supplier) {
			(void)output(OUT_UNICAST, from, sifp, rip, size);
		}
		return;

	case RIPCMD_TRACEON:
	case RIPCMD_TRACEOFF:
		/* verify message came from a privileged port */
		if (ntohs(from->sin_port) > IPPORT_RESERVED) {
			msglog("trace command from untrusted port on %s",
			       naddr_ntoa(FROM_NADDR));
			return;
		}
		if (aifp == 0) {
			msglog("trace command from unknown router %s",
			       naddr_ntoa(FROM_NADDR));
			return;
		}
		if (rip->rip_cmd == RIPCMD_TRACEON) {
			rip->rip_tracefile[size-4] = '\0';
			trace_on((char*)rip->rip_tracefile, 0);
		} else {
			trace_off("tracing turned off by %s\n",
				  naddr_ntoa(FROM_NADDR));
		}
		return;

	case RIPCMD_RESPONSE:
		if (size%sizeof(*n) != sizeof(struct rip)%sizeof(*n)) {
			if (from->sin_addr.s_addr != bad_len)
				msglog("response of bad length (%d) from %s",
				       size, naddr_ntoa(FROM_NADDR));
			bad_len = from->sin_addr.s_addr;
		}

		/* verify message came from a router */
		if (from->sin_port != ntohs(RIP_PORT)) {
			trace_pkt("discard RIP response from unknown port\n");
			return;
		}

		if (rip_sock < 0) {
			trace_pkt("discard response while RIP off\n");
			return;
		}

		/* Are we talking to ourself or a remote gateway?
		 */
		ifp1 = ifwithaddr(FROM_NADDR, 0, 1);
		if (ifp1) {
			if (ifp1->int_state & IS_REMOTE) {
				if (ifp1->int_state & IS_PASSIVE) {
					msglog("bogus input from %s on"
					       " supposedly passive %s",
					       naddr_ntoa(FROM_NADDR),
					       ifp1->int_name);
				} else {
					ifp1->int_act_time = now.tv_sec;
					if (if_ok(ifp1, "remote "))
						addrouteforif(ifp1);
				}
			} else {
				trace_pkt("discard our own RIP response\n");
			}
			return;
		}

		/* Check the router from which message originated. We accept
		 * routing packets from routers directly connected via
		 * broadcast or point-to-point networks, and from
		 * those listed in /etc/gateways.
		 */
		if (!aifp) {
			if (from->sin_addr.s_addr != unk_router)
				msglog("discard packet from unknown router %s"
				       " or via unidentified interface",
				       naddr_ntoa(FROM_NADDR));
			unk_router = from->sin_addr.s_addr;
			return;
		}
		if (aifp->int_state & IS_PASSIVE) {
			trace_act("discard packet from %s"
				  " via passive interface %s\n",
				  naddr_ntoa(FROM_NADDR),
				  aifp->int_name);
			return;
		}

		/* Check required version
		 */
		if (((aifp->int_state & IS_NO_RIPV1_IN)
		     && rip->rip_vers == RIPv1)
		    || ((aifp->int_state & IS_NO_RIPV2_IN)
			&& rip->rip_vers != RIPv1)) {
			trace_pkt("discard RIPv%d response\n",
				  rip->rip_vers);
			return;
		}

		/* Ignore routes via dead interface.
		 */
		if (aifp->int_state & IS_BROKE) {
			trace_pkt("discard response via broken interface %s\n",
				  aifp->int_name);
			return;
		}

		/* Authenticate the packet if we have a secret.
		 */
		if (aifp->int_passwd[0] != '\0') {
			if (n >= lim
			    || n->n_family != RIP_AF_AUTH
			    || ((struct netauth*)n)->a_type != RIP_AUTH_PW) {
				if (from->sin_addr.s_addr != use_auth)
					msglog("missing password from %s",
					       naddr_ntoa(FROM_NADDR));
				use_auth = from->sin_addr.s_addr;
				return;

			} else if (0 != bcmp(((struct netauth*)n)->au.au_pw,
					     aifp->int_passwd,
					     sizeof(aifp->int_passwd))) {
				if (from->sin_addr.s_addr != use_auth)
					msglog("bad password from %s",
					       naddr_ntoa(FROM_NADDR));
				use_auth = from->sin_addr.s_addr;
				return;
			}
		}

		for (; n < lim; n++) {
			if (n->n_family == RIP_AF_AUTH)
				continue;

			NTOHL(n->n_metric);
			dst = n->n_dst;
			if (n->n_family != RIP_AF_INET
			    && (n->n_family != RIP_AF_UNSPEC
				|| dst != RIP_DEFAULT)) {
				if (from->sin_addr.s_addr != bad_router)
					msglog("route from %s to unsupported"
					       " address family %d,"
					       " destination %s",
					       naddr_ntoa(FROM_NADDR),
					       n->n_family,
					       naddr_ntoa(dst));
				bad_router = from->sin_addr.s_addr;
				continue;
			}
			if (!check_dst(dst)) {
				if (from->sin_addr.s_addr != bad_router)
					msglog("bad destination %s from %s",
					       naddr_ntoa(dst),
					       naddr_ntoa(FROM_NADDR));
				bad_router = from->sin_addr.s_addr;
				return;
			}
			if (n->n_metric == 0
			    || n->n_metric > HOPCNT_INFINITY) {
				if (from->sin_addr.s_addr != bad_router)
					msglog("bad metric %d from %s"
					       " for destination %s",
					       n->n_metric,
					       naddr_ntoa(FROM_NADDR),
					       naddr_ntoa(dst));
				bad_router = from->sin_addr.s_addr;
				return;
			}

			/* Notice the next-hop.
			 */
			gate = from->sin_addr.s_addr;
			if (n->n_nhop != 0) {
				if (rip->rip_vers == RIPv2) {
					n->n_nhop = 0;
				} else {
				    /* Use it only if it is valid. */
				    if (on_net(n->n_nhop,
					       aifp->int_net, aifp->int_mask)
					&& check_dst(n->n_nhop)) {
					    gate = n->n_nhop;
				    } else {
					if (bad_nhop != from->sin_addr.s_addr)
						msglog("router %s to %s has"
						       " bad next hop %s",
						       naddr_ntoa(FROM_NADDR),
						       naddr_ntoa(dst),
						       naddr_ntoa(n->n_nhop));
					bad_nhop = from->sin_addr.s_addr;
					n->n_nhop = 0;
				    }
				}
			}

			if (rip->rip_vers == RIPv1
			    || 0 == (mask = ntohl(n->n_mask))) {
				mask = ripv1_mask_host(dst,aifp);
			} else if ((ntohl(dst) & ~mask) != 0) {
				if (bad_mask != from->sin_addr.s_addr) {
					msglog("router %s sent bad netmask"
					       " %#x with %s",
					       naddr_ntoa(FROM_NADDR),
					       mask,
					       naddr_ntoa(dst));
					bad_mask = from->sin_addr.s_addr;
				}
				continue;
			}
			if (rip->rip_vers == RIPv1)
				n->n_tag = 0;

			/* Adjust metric according to incoming interface..
			 */
			n->n_metric += aifp->int_metric;
			if (n->n_metric > HOPCNT_INFINITY)
				n->n_metric = HOPCNT_INFINITY;

			/* Recognize and ignore a default route we faked
			 * which is being sent back to us by a machine with
			 * broken split-horizon.
			 * Be a little more paranoid than that, and reject
			 * default routes with the same metric we advertised.
			 */
			if (aifp->int_d_metric != 0
			    && dst == RIP_DEFAULT
			    && n->n_metric >= aifp->int_d_metric)
				continue;

			/* We can receive aggregated RIPv2 routes that must
			 * be broken down before they are transmitted by
			 * RIPv1 via an interface on a subnet.
			 * We might also receive the same routes aggregated
			 * via other RIPv2 interfaces.
			 * This could cause duplicate routes to be sent on
			 * the RIPv1 interfaces.  "Longest matching variable
			 * length netmasks" lets RIPv2 listeners understand,
			 * but breaking down the aggregated routes for RIPv1
			 * listeners can produce duplicate routes.
			 *
			 * Breaking down aggregated routes here bloats
			 * the daemon table, but does not hurt the kernel
			 * table, since routes are always aggregated for
			 * the kernel.
			 *
			 * Notice that this does not break down network
			 * routes corresponding to subnets.  This is part
			 * of the defense against RS_NET_SYN.
			 */
			if (have_ripv1_out
			    && (v1_mask = ripv1_mask_net(dst,0)) > mask
			    && (((rt = rtget(dst,mask)) == 0
				 || !(rt->rt_state & RS_NET_SYN)))) {
				ddst_h = v1_mask & -v1_mask;
				i = (v1_mask & ~mask)/ddst_h;
				if (i >= 511) {
					/* Punt if we would have to generate
					 * an unreasonable number of routes.
					 */
#ifdef DEBUG
					msglog("accept %s from %s as 1"
					       " instead of %d routes",
					       addrname(dst,mask,0),
					       naddr_ntoa(FROM_NADDR),
					       i+1);
#endif
					i = 0;
				} else {
					mask = v1_mask;
				}
			} else {
				i = 0;
			}

			for (;;) {
				input_route(aifp, FROM_NADDR,
					    dst, mask, gate, n);
				if (i-- == 0)
					break;
				dst = htonl(ntohl(dst) + ddst_h);
			}
		}
		break;
	}
}


/* Process a single input route.
 */
static void
input_route(struct interface *ifp,
	    naddr from,
	    naddr dst,
	    naddr mask,
	    naddr gate,
	    struct netinfo *n)
{
	int i;
	struct rt_entry *rt;
	struct rt_spare *rts, *rts0;
	struct interface *ifp1;
	time_t new_time;


	/* See if the other guy is telling us to send our packets to him.
	 * Sometimes network routes arrive over a point-to-point link for
	 * the network containing the address(es) of the link.
	 *
	 * If our interface is broken, switch to using the other guy.
	 */
	ifp1 = ifwithaddr(dst, 1, 1);
	if (ifp1 != 0
	    && !(ifp1->int_state & IS_BROKE))
		return;

	/* Look for the route in our table.
	 */
	rt = rtget(dst, mask);

	/* Consider adding the route if we do not already have it.
	 */
	if (rt == 0) {
		/* Ignore unknown routes being poisoned.
		 */
		if (n->n_metric == HOPCNT_INFINITY)
			return;

		/* Ignore the route if it points to us */
		if (n->n_nhop != 0
		    && 0 != ifwithaddr(n->n_nhop, 1, 0))
			return;

		/* If something has not gone crazy and tried to fill
		 * our memory, accept the new route.
		 */
		if (total_routes < MAX_ROUTES)
			rtadd(dst, mask, gate, from, n->n_metric,
			      n->n_tag, 0, ifp);
		return;
	}

	/* We already know about the route.  Consider this update.
	 *
	 * If (rt->rt_state & RS_NET_SYN), then this route
	 * is the same as a network route we have inferred
	 * for subnets we know, in order to tell RIPv1 routers
	 * about the subnets.
	 *
	 * It is impossible to tell if the route is coming
	 * from a distant RIPv2 router with the standard
	 * netmask because that router knows about the entire
	 * network, or if it is a round-about echo of a
	 * synthetic, RIPv1 network route of our own.
	 * The worst is that both kinds of routes might be
	 * received, and the bad one might have the smaller
	 * metric.  Partly solve this problem by never
	 * aggregating into such a route.  Also keep it
	 * around as long as the interface exists.
	 */

	rts0 = rt->rt_spares;
	for (rts = rts0, i = NUM_SPARES; i != 0; i--, rts++) {
		if (rts->rts_router == from)
			break;
		/* Note the worst slot to reuse,
		 * other than the current slot.
		 */
		if (rts0 == rt->rt_spares
		    || BETTER_LINK(rt, rts0, rts))
			rts0 = rts;
	}
	if (i != 0) {
		/* Found the router
		 */
		int old_metric = rts->rts_metric;

		/* Keep poisoned routes around only long enough to pass
		 * the poison on.  Get a new timestamp for good routes.
		 */
		new_time =((old_metric == HOPCNT_INFINITY)
			   ? rts->rts_time
			   : now.tv_sec);

		/* If this is an update for the router we currently prefer,
		 * then note it.
		 */
		if (i == NUM_SPARES) {
			rtchange(rt,rt->rt_state, gate,rt->rt_router,
				 n->n_metric, n->n_tag, ifp, new_time, 0);
			/* If the route got worse, check for something better.
			 */
			if (n->n_metric > old_metric)
				rtswitch(rt, 0);
			return;
		}

		/* This is an update for a spare route.
		 * Finished if the route is unchanged.
		 */
		if (rts->rts_gate == gate
		    && old_metric == n->n_metric
		    && rts->rts_tag == n->n_tag) {
			rts->rts_time = new_time;
			return;
		}

	} else {
		/* The update is for a route we know about,
		 * but not from a familiar router.
		 *
		 * Ignore the route if it points to us.
		 */
		if (n->n_nhop != 0
		    && 0 != ifwithaddr(n->n_nhop, 1, 0))
			return;

		rts = rts0;

		/* Save the route as a spare only if it has
		 * a better metric than our worst spare.
		 * This also ignores poisoned routes (those
		 * received with metric HOPCNT_INFINITY).
		 */
		if (n->n_metric >= rts->rts_metric)
			return;

		new_time = now.tv_sec;
	}

	trace_upslot(rt, rts, gate, from, ifp, n->n_metric,n->n_tag, new_time);

	rts->rts_gate = gate;
	rts->rts_router = from;
	rts->rts_metric = n->n_metric;
	rts->rts_tag = n->n_tag;
	rts->rts_time = new_time;
	rts->rts_ifp = ifp;

	/* try to switch to a better route */
	rtswitch(rt, rts);
}
