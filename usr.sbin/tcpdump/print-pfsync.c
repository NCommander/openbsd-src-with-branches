/*	$OpenBSD: print-pfsync.c,v 1.11 2003/11/08 06:46:44 mcbride Exp $	*/

/*
 * Copyright (c) 2002 Michael Shalayeff
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /cvs/src/usr.sbin/tcpdump/print-pfsync.c,v 1.11 2003/11/08 06:46:44 mcbride Exp $";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>

#ifdef __STDC__
struct rtentry;
#endif
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#include <net/pfvar.h>
#include <net/if_pfsync.h>

#include <ctype.h>
#include <netdb.h>
#include <pcap.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#include "pfctl_parser.h"
#include "pfctl.h"

const char *pfsync_acts[] = { PFSYNC_ACTIONS };

void
pfsync_if_print(u_char *user, const struct pcap_pkthdr *h,
     register const u_char *p)
{
	/*u_int length = h->len;*/
	u_int caplen = h->caplen;
	struct pfsync_header *hdr;
	struct pf_state *s;
	int i, flags;

	ts_print(&h->ts);

	if (caplen < PFSYNC_HDRLEN) {
		printf("[|pfsync]");
		goto out;
	}

	packetp = p;
	snapend = p + caplen;

	hdr = (struct pfsync_header *)p;
	if (eflag)
		printf("version %d count %d: ",
		    hdr->version, hdr->count);

	if (hdr->action < PFSYNC_ACT_MAX)
		printf("%s:\n", pfsync_acts[hdr->action]);
	else
		printf("%d?:\n", hdr->action);

	flags = 0;
	if (vflag)
		flags |= PF_OPT_VERBOSE;
	if (!nflag)
		flags |= PF_OPT_USEDNS;

	for (i = 1, s = (struct pf_state *)(p + PFSYNC_HDRLEN);
	    i <= hdr->count && PFSYNC_HDRLEN + i * sizeof(*s) <= caplen;
	    i++, s++) {
		struct pf_state st;

		bcopy(&s->lan, &st.lan, sizeof(st.lan));
		bcopy(&s->gwy, &st.gwy, sizeof(st.gwy));
		bcopy(&s->ext, &st.ext, sizeof(st.ext));
		pf_state_peer_ntoh(&s->src, &st.src);
		pf_state_peer_ntoh(&s->dst, &st.dst);
		st.rule.nr = ntohl(s->rule.nr);
		st.anchor.nr = ntohl(s->anchor.nr);
		bcopy(&s->rt_addr, &st.rt_addr, sizeof(st.rt_addr));
		st.creation = ntohl(s->creation);
		st.expire = ntohl(s->expire);
		st.packets[0] = ntohl(s->packets[0]);
		st.packets[1] = ntohl(s->packets[1]);
		st.bytes[0] = ntohl(s->bytes[0]);
		st.bytes[1] = ntohl(s->bytes[1]);
		st.af = s->af;
		st.proto = s->proto;
		st.direction = s->direction;
		st.log = s->log;
		st.allow_opts = s->allow_opts;

		print_state(&st, flags);
	}
out:
	if (xflag)
		default_print((const u_char *)hdr, caplen);
	putchar('\n');
}
