/*	$OpenBSD: pf_print_state.c,v 1.1 2002/06/06 22:22:44 mickey Exp $	*/

/*
 * Copyright (c) 2001 Daniel Hartmeier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#define TCPSTATES
#include <netinet/tcp_fsm.h>
#include <net/pfvar.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <netdb.h>
#include <stdarg.h>
#include <errno.h>
#include <err.h>

#include "pfctl_parser.h"
#include "pf_print_state.h"

int
unmask(struct pf_addr *m, u_int8_t af)
{
	int i = 31, j = 0, b = 0, msize;
	u_int32_t tmp;

	if (af == AF_INET)
		msize = 1;
	else
		msize = 4;
	while (j < msize && m->addr32[j] == 0xffffffff) {
		b += 32;
		j++;
	}
	if (j < msize) {
		tmp = ntohl(m->addr32[j]);
		for (i = 31; tmp & (1 << i); --i)
			b++;
	}
	return (b);
}

void
print_addr(struct pf_addr_wrap *addr, struct pf_addr *mask, u_int8_t af)
{
	char buf[48];

	if (addr->addr_dyn != NULL)
		printf("(%s)", addr->addr.pfa.ifname);
	else {
		if (inet_ntop(af, &addr->addr, buf, sizeof(buf)) == NULL)
			printf("?");
		else
			printf("%s", buf);
	}
	if (mask != NULL) {
		int bits = unmask(mask, af);

		if (bits != (af == AF_INET ? 32 : 128))
			printf("/%u", bits);
	}
}

void
print_name(struct pf_addr *addr, struct pf_addr *mask, int af)
{
	char buf[48];
	struct hostent *hp;

	if (inet_ntop(af, addr, buf, sizeof(buf)) == NULL)
		printf("?");
	else {
		hp = getpfhostname(buf);
		printf("%s", hp->h_name);
	}
	if (mask != NULL) {
		if (!PF_AZERO(mask, af))
			printf("/%u", unmask(mask, af));
	}
}

void
print_host(struct pf_state_host *h, u_int8_t af, int opts)
{
	u_int16_t p = ntohs(h->port);

	if (opts & PF_OPT_USEDNS)
		print_name(&h->addr, NULL, af);
	else {
		struct pf_addr_wrap aw;

		aw.addr = h->addr;
		aw.addr_dyn = NULL;
		print_addr(&aw, NULL, af);
	}

	if (p) {
		if (af == AF_INET)
			printf(":%u", p);
		else
			printf("[%u]", p);
	}
}

void
print_seq(struct pf_state_peer *p)
{
	if (p->seqdiff)
		printf("[%u + %u](+%u)", p->seqlo, p->seqhi - p->seqlo,
		    p->seqdiff);
	else
		printf("[%u + %u]", p->seqlo, p->seqhi - p->seqlo);
}

void
print_state(struct pf_state *s, int opts)
{
	struct pf_state_peer *src, *dst;
	struct protoent *p;
	u_int8_t hrs, min, sec;

	if (s->direction == PF_OUT) {
		src = &s->src;
		dst = &s->dst;
	} else {
		src = &s->dst;
		dst = &s->src;
	}
	if ((p = getprotobynumber(s->proto)) != NULL)
		printf("%s ", p->p_name);
	else
		printf("%u ", s->proto);
	if (PF_ANEQ(&s->lan.addr, &s->gwy.addr, s->af) ||
	    (s->lan.port != s->gwy.port)) {
		print_host(&s->lan, s->af, opts);
		if (s->direction == PF_OUT)
			printf(" -> ");
		else
			printf(" <- ");
	}
	print_host(&s->gwy, s->af, opts);
	if (s->direction == PF_OUT)
		printf(" -> ");
	else
		printf(" <- ");
	print_host(&s->ext, s->af, opts);

	printf("    ");
	if (s->proto == IPPROTO_TCP) {
		if (src->state <= TCPS_TIME_WAIT &&
		    dst->state <= TCPS_TIME_WAIT) {
			printf("   %s:%s\n", tcpstates[src->state],
			    tcpstates[dst->state]);
		} else {
			printf("   <BAD STATE LEVELS>\n");
		}
		if (opts & PF_OPT_VERBOSE) {
			printf("   ");
			print_seq(src);
			printf("  ");
			print_seq(dst);
			printf("\n");
		}
	} else if (s->proto == IPPROTO_UDP && src->state < PFUDPS_NSTATES &&
	    dst->state < PFUDPS_NSTATES) {
		const char *states[] = PFUDPS_NAMES;
		printf("   %s:%s\n", states[src->state], states[dst->state]);
	} else if (s->proto != IPPROTO_ICMP && src->state < PFOTHERS_NSTATES &&
	    dst->state < PFOTHERS_NSTATES) {
		/* XXX ICMP doesn't really have state levels */
		const char *states[] = PFOTHERS_NAMES;
		printf("   %s:%s\n", states[src->state], states[dst->state]);
	} else {
		printf("   %u:%u\n", src->state, dst->state);
	}

	if (opts & PF_OPT_VERBOSE) {
		sec = s->creation % 60;
		s->creation /= 60;
		min = s->creation % 60;
		s->creation /= 60;
		hrs = s->creation;
		printf("   age %.2u:%.2u:%.2u", hrs, min, sec);
		sec = s->expire % 60;
		s->expire /= 60;
		min = s->expire % 60;
		s->expire /= 60;
		hrs = s->expire;
		printf(", expires in %.2u:%.2u:%.2u", hrs, min, sec);
		printf(", %u pkts, %u bytes", s->packets, s->bytes);
		if (s->rule.nr != USHRT_MAX)
			printf(", rule %u", s->rule.nr);
		printf("\n");
	}
}

struct hostent *
getpfhostname(const char *addr_str)
{
	in_addr_t		 addr_num;
	struct hostent		*hp;
	static struct hostent	 myhp;

	addr_num = inet_addr(addr_str);
	if (addr_num == INADDR_NONE) {
		myhp.h_name = (char *)addr_str;
		hp = &myhp;
		return (hp);
	}
	hp = gethostbyaddr((char *)&addr_num, sizeof(addr_num), AF_INET);
	if (hp == NULL) {
		myhp.h_name = (char *)addr_str;
		hp = &myhp;
	}
	return (hp);
}
