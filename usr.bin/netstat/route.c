/*	$OpenBSD: route.c,v 1.29 1999/09/22 05:10:04 deraadt Exp $	*/
/*	$NetBSD: route.c,v 1.15 1996/05/07 02:55:06 thorpej Exp $	*/

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
static char sccsid[] = "from: @(#)route.c	8.3 (Berkeley) 3/9/94";
#else
static char *rcsid = "$OpenBSD: route.c,v 1.29 1999/09/22 05:10:04 deraadt Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/mbuf.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#define _KERNEL
#include <net/route.h>
#undef _KERNEL
#include <netinet/in.h>
#include <arpa/inet.h>

#include <netns/ns.h>

#include <netipx/ipx.h>

#include <netatalk/at.h>

#include <sys/sysctl.h>

#include <arpa/inet.h>

#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef INET
#define INET
#endif

#include <sys/socket.h>
#include <netinet/ip_ipsp.h>
#include "netstat.h"

#define kget(p, d) (kread((u_long)(p), (char *)&(d), sizeof (d)))

/* alignment constraint for routing socket */
#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

/*
 * Definitions for showing gateway flags.
 */
struct bits {
	short	b_mask;
	char	b_val;
} bits[] = {
	{ RTF_UP,	'U' },
	{ RTF_GATEWAY,	'G' },
	{ RTF_HOST,	'H' },
	{ RTF_REJECT,	'R' },
	{ RTF_DYNAMIC,	'D' },
	{ RTF_MODIFIED,	'M' },
	{ RTF_DONE,	'd' }, /* Completed -- for routing messages only */
	{ RTF_MASK,	'm' }, /* Mask Present -- for routing messages only */
	{ RTF_CLONING,	'C' },
	{ RTF_XRESOLVE,	'X' },
	{ RTF_LLINFO,	'L' },
	{ RTF_STATIC,	'S' },
	{ RTF_PROTO1,	'1' },
	{ RTF_PROTO2,	'2' },
	{ 0 }
};

static union {
	struct		sockaddr u_sa;
	u_int32_t	u_data[64];
	int		u_dummy;	/* force word-alignment */
} pt_u;

int	do_rtent = 0;
struct	rtentry rtentry;
struct	radix_node rnode;
struct	radix_mask rmask;

int	NewTree = 0;

static struct sockaddr *kgetsa __P((struct sockaddr *));
static void p_tree __P((struct radix_node *));
static void p_rtnode __P(());
static void ntreestuff __P(());
static void np_rtentry __P((struct rt_msghdr *));
static void p_sockaddr __P((struct sockaddr *, struct sockaddr *, int, int));
static void p_flags __P((int, char *));
static void p_rtentry __P((struct rtentry *));
static void encap_print __P((struct rtentry *));

/*
 * Print routing tables.
 */
void
routepr(rtree)
	u_long rtree;
{
	struct radix_node_head *rnh, head;
	int i;

	printf("Routing tables\n");

	if (Aflag == 0 && NewTree)
		ntreestuff();
	else {
		if (rtree == 0) {
			printf("rt_tables: symbol not in namelist\n");
			return;
		}

		kget(rtree, rt_tables);
		for (i = 0; i <= AF_MAX; i++) {
			if ((rnh = rt_tables[i]) == 0)
				continue;
			kget(rnh, head);
			if (i == AF_UNSPEC) {
				if (Aflag && af == 0) {
					printf("Netmasks:\n");
					p_tree(head.rnh_treetop);
				}
			} else if (af == AF_UNSPEC || af == i) {
				pr_family(i);
				do_rtent = 1;
				if (i != PF_KEY)
					pr_rthdr(i);
				else
					pr_encaphdr();
				p_tree(head.rnh_treetop);
			}
		}
	}
}

/*
 * Print address family header before a section of the routing table.
 */
void
pr_family(af)
	int af;
{
	char *afname;

	switch (af) {
	case AF_INET:
		afname = "Internet";
		break;
#ifdef INET6
	case AF_INET6:
		afname = "Internet6";
		break;
#endif 
	case AF_NS:
		afname = "XNS";
		break;
	case AF_IPX:
		afname = "IPX";
		break;
	case AF_ISO:
		afname = "ISO";
		break;
	case AF_CCITT:
		afname = "X.25";
		break;
	case PF_KEY:
		afname = "Encap";
		break;
	case AF_APPLETALK:
		afname = "AppleTalk";
		break;
	default:
		afname = NULL;
		break;
	}
	if (afname)
		printf("\n%s:\n", afname);
	else
		printf("\nProtocol Family %d:\n", af);
}

/* column widths; each followed by one space */
#ifndef INET6
#define	WID_DST(af)	18	/* width of destination column */
#define	WID_GW(af)	18	/* width of gateway column */
#else
/* width of destination/gateway column */
#ifdef KAME_SCOPEID
/* strlen("fe80::aaaa:bbbb:cccc:dddd@gif0") == 30, strlen("/128") == 4 */
#define	WID_DST(af)	((af) == AF_INET6 ? (nflag ? 34 : 18) : 18)
#define	WID_GW(af)	((af) == AF_INET6 ? (nflag ? 30 : 18) : 18)
#else
/* strlen("fe80::aaaa:bbbb:cccc:dddd") == 25, strlen("/128") == 4 */
#define	WID_DST(af)	((af) == AF_INET6 ? (nflag ? 29 : 18) : 18)
#define	WID_GW(af)	((af) == AF_INET6 ? (nflag ? 25 : 18) : 18)
#endif
#endif /* INET6 */

/*
 * Print header for routing table columns.
 */
void
pr_rthdr(af)
	int af;
{

	if (Aflag)
		printf("%-*.*s ", PLEN, PLEN, "Address");
	printf("%-*.*s %-*.*s %-6.6s  %6.6s  %6.6s %6.6s  %s\n",
		WID_DST(af), WID_DST(af), "Destination",
		WID_GW(af), WID_GW(af), "Gateway",
		"Flags", "Refs", "Use", "Mtu", "Interface");
}

/*
 * Print header for PF_KEY entries.
 */
void
pr_encaphdr()
{
	if (Aflag)
		printf("%-*s ", PLEN, "Address");
	printf("%-18s %-5s %-18s %-5s %-5s %-22s\n",
	    "Source", "Port", "Destination", 
	    "Port", "Proto", "SA(Address/SPI/Proto)");
}

static struct sockaddr *
kgetsa(dst)
	register struct sockaddr *dst;
{

	kget(dst, pt_u.u_sa);
	if (pt_u.u_sa.sa_len > sizeof (pt_u.u_sa))
		kread((u_long)dst, (char *)pt_u.u_data, pt_u.u_sa.sa_len);
	return (&pt_u.u_sa);
}

static void
p_tree(rn)
	struct radix_node *rn;
{

again:
	kget(rn, rnode);
	if (rnode.rn_b < 0) {
		if (Aflag)
			printf("%-16p ", rn);
		if (rnode.rn_flags & RNF_ROOT) {
			if (Aflag)
				printf("(root node)%s",
				    rnode.rn_dupedkey ? " =>\n" : "\n");
		} else if (do_rtent) {
			kget(rn, rtentry);
			p_rtentry(&rtentry);
			if (Aflag)
				p_rtnode();
		} else {
			p_sockaddr(kgetsa((struct sockaddr *)rnode.rn_key),
			    0, 0, 44);
			putchar('\n');
		}
		if ((rn = rnode.rn_dupedkey))
			goto again;
	} else {
		if (Aflag && do_rtent) {
			printf("%-16p ", rn);
			p_rtnode();
		}
		rn = rnode.rn_r;
		p_tree(rnode.rn_l);
		p_tree(rn);
	}
}

char	nbuf[25];

static void
p_rtnode()
{
	struct radix_mask *rm = rnode.rn_mklist;

	if (rnode.rn_b < 0) {
		if (rnode.rn_mask) {
			printf("\t  mask ");
			p_sockaddr(kgetsa((struct sockaddr *)rnode.rn_mask),
			    0, 0, -1);
		} else if (rm == 0)
			return;
	} else {
		snprintf(nbuf, sizeof nbuf, "(%d)", rnode.rn_b);
		printf("%6.6s %16p : %16p", nbuf, rnode.rn_l,
		    rnode.rn_r);
	}
	while (rm) {
		kget(rm, rmask);
		snprintf(nbuf, sizeof nbuf, " %d refs, ", rmask.rm_refs);
		printf(" mk = %16p {(%d),%s",
			rm, -1 - rmask.rm_b, rmask.rm_refs ? nbuf : " ");
		p_sockaddr(kgetsa((struct sockaddr *)rmask.rm_mask), 0, 0, -1);
		putchar('}');
		if ((rm = rmask.rm_mklist))
			printf(" ->");
	}
	putchar('\n');
}

static void
ntreestuff()
{
	size_t needed;
	int mib[6];
	char *buf, *next, *lim;
	register struct rt_msghdr *rtm;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
		perror("route-sysctl-estimate");
		exit(1);
	}
	if ((buf = malloc(needed)) == 0) {
		printf("out of space\n");
		exit(1);
	}
        if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
		perror("sysctl of routing table");
		exit(1);
	}
	lim = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		np_rtentry(rtm);
	}
}

static void
np_rtentry(rtm)
	register struct rt_msghdr *rtm;
{
	register struct sockaddr *sa = (struct sockaddr *)(rtm + 1);
#ifdef notdef
	static int masks_done, banner_printed;
#endif
	static int old_af;
	int af = 0, interesting = RTF_UP | RTF_GATEWAY | RTF_HOST;

#ifdef notdef
	/* for the moment, netmasks are skipped over */
	if (!banner_printed) {
		printf("Netmasks:\n");
		banner_printed = 1;
	}
	if (masks_done == 0) {
		if (rtm->rtm_addrs != RTA_DST ) {
			masks_done = 1;
			af = sa->sa_family;
		}
	} else
#endif
		af = sa->sa_family;
	if (af != old_af) {
		pr_family(af);
		old_af = af;
	}
	if (rtm->rtm_addrs == RTA_DST)
		p_sockaddr(sa, 0, 0, 36);
	else {
		p_sockaddr(sa, 0, rtm->rtm_flags, 16);
		sa = (struct sockaddr *)(ROUNDUP(sa->sa_len) + (char *)sa);
		p_sockaddr(sa, 0, 0, 18);
	}
	p_flags(rtm->rtm_flags & interesting, "%-6.6s ");
	putchar('\n');
}

static void
p_sockaddr(sa, mask, flags, width)
	struct sockaddr *sa, *mask;
	int flags, width;
{
	char workbuf[128], *cplim;
	register char *cp = workbuf;
	size_t n;

	switch(sa->sa_family) {
	case AF_INET:
	    {
		register struct sockaddr_in *sin = (struct sockaddr_in *)sa;
		register struct sockaddr_in *msin = (struct sockaddr_in *)mask;

		cp = (sin->sin_addr.s_addr == 0) ? "default" :
		      ((flags & RTF_HOST) ?
			routename(sin->sin_addr.s_addr) :
			netname(sin->sin_addr.s_addr, msin->sin_addr.s_addr));

		break;
	    }

#ifdef INET6
	case AF_INET6:
	    {
		struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)sa;
#ifdef KAME_SCOPEID
		struct in6_addr *in6 = &sa6->sin6_addr;

		/*
		 * XXX: This is a special workaround for KAME kernels.
		 * sin6_scope_id field of SA should be set in the future.
		 */
		if (IN6_IS_ADDR_LINKLOCAL(in6) ||
		    IN6_IS_ADDR_MC_LINKLOCAL(in6)) {
		    /* XXX: override is ok? */
		    sa6->sin6_scope_id = (u_int32_t)ntohs(*(u_short *)&in6->s6_addr[2]);
		    *(u_short *)&in6->s6_addr[2] = 0;
		}
#endif

		if (flags & RTF_HOST)
			cp = routename6(sa6);
		else if (mask) {
			cp = netname6(sa6,
				&((struct sockaddr_in6 *)mask)->sin6_addr);
		} else
			cp = netname6(sa6, NULL);
		break;
	    }
#endif 

	case AF_NS:
		cp = ns_print(sa);
		break;

	case AF_IPX:
		cp = ipx_print(sa);
		break;
		
	case AF_LINK:
	    {
		register struct sockaddr_dl *sdl = (struct sockaddr_dl *)sa;

		if (sdl->sdl_nlen == 0 && sdl->sdl_alen == 0 &&
		    sdl->sdl_slen == 0)
			(void) snprintf(workbuf, sizeof workbuf,
			    "link#%d", sdl->sdl_index);
		else switch (sdl->sdl_type) {
		case IFT_ETHER:
		    {
			register int i;
			register u_char *lla = (u_char *)sdl->sdl_data +
			    sdl->sdl_nlen;

			cplim = "";
			for (i = 0; i < sdl->sdl_alen; i++, lla++) {
				n = snprintf(cp,
				    workbuf + sizeof (workbuf) - cp,
				    "%s%x", cplim, *lla);
				if (n >= workbuf + sizeof (workbuf) - cp)
					n = workbuf + sizeof (workbuf) - cp - 1;
				cp += n;
				cplim = ":";
			}
			cp = workbuf;
			break;
		    }
		default:
			cp = link_ntoa(sdl);
			break;
		}
		break;
	    }

	case AF_APPLETALK:
	    {
		/* XXX could do better */
		cp = atalk_print(sa,11);
		break;
	    }
	default:
	    {
		register u_char *s = (u_char *)sa->sa_data, *slim;

		slim = sa->sa_len + (u_char *) sa;
		cplim = cp + sizeof(workbuf) - 6;
		n = snprintf(cp, cplim - cp, "(%d)", sa->sa_family);
		if (n >= cplim - cp)
			n = cplim - cp - 1;
		cp += n;
		while (s < slim && cp < cplim) {
			n = snprintf(cp, workbuf + sizeof (workbuf) - cp,
			    " %02x", *s++);
			if (n >= workbuf + sizeof (workbuf) - cp)
				n = workbuf + sizeof (workbuf) - cp - 1;
			cp += n;
			if (s < slim) {
				n = snprintf(cp,
				    workbuf + sizeof (workbuf) - cp,
				    "%02x", *s++);
				if (n >= workbuf + sizeof (workbuf) - cp)
					n = workbuf + sizeof (workbuf) - cp - 1;
				cp += n;
			}
		}
		cp = workbuf;
	    }
	}
	if (width < 0 )
		printf("%s ", cp);
	else {
		if (nflag)
			printf("%-*s ", width, cp);
		else
			printf("%-*.*s ", width, width, cp);
	}
}

static void
p_flags(f, format)
	register int f;
	char *format;
{
	char name[33], *flags;
	register struct bits *p = bits;

	for (flags = name; p->b_mask; p++)
		if (p->b_mask & f)
			*flags++ = p->b_val;
	*flags = '\0';
	printf(format, name);
}

static void
p_rtentry(rt)
	register struct rtentry *rt;
{
	static struct ifnet ifnet, *lastif;
	struct sockaddr_storage sock1, sock2;
	struct sockaddr *sa = (struct sockaddr *)&sock1;
	struct sockaddr *mask = (struct sockaddr *)&sock2;
	
	bcopy(kgetsa(rt_key(rt)), sa, sizeof(struct sockaddr));
	if (sa->sa_len > sizeof(struct sockaddr))
		bcopy(kgetsa(rt_key(rt)), sa, sa->sa_len);

	if (sa->sa_family == PF_KEY) {
		encap_print(rt);
		return;
	}

	if (rt_mask(rt)) {
		bcopy(kgetsa(rt_mask(rt)), mask, sizeof(struct sockaddr));
		if (sa->sa_len > sizeof(struct sockaddr))
			bcopy(kgetsa(rt_mask(rt)), mask, sa->sa_len);
	} else
		mask = 0;
	
	p_sockaddr(sa, mask, rt->rt_flags, WID_DST(sa->sa_family));
	p_sockaddr(kgetsa(rt->rt_gateway), 0, RTF_HOST, WID_GW(sa->sa_family));
	p_flags(rt->rt_flags, "%-6.6s ");
	printf("%6d %8ld ", rt->rt_refcnt, rt->rt_use);
	if (rt->rt_rmx.rmx_mtu)
		printf("%6ld ", rt->rt_rmx.rmx_mtu);
	else
		printf("%6s ", "-");
	if (rt->rt_ifp) {
		if (rt->rt_ifp != lastif) {
			kget(rt->rt_ifp, ifnet);
			lastif = rt->rt_ifp;
		}
		printf(" %.16s%s", ifnet.if_xname,
			rt->rt_nodes[0].rn_dupedkey ? " =>" : "");
	}
	putchar('\n');
}

char *
routename(in)
	in_addr_t in;
{
	register char *cp;
	static char line[MAXHOSTNAMELEN];
	struct hostent *hp;
	static char domain[MAXHOSTNAMELEN];
	static int first = 1;

	if (first) {
		first = 0;
		if (gethostname(domain, sizeof domain) == 0 &&
		    (cp = strchr(domain, '.')))
			(void) strcpy(domain, cp + 1);
		else
			domain[0] = 0;
	}
	cp = 0;
	if (!nflag) {
		hp = gethostbyaddr((char *)&in, sizeof (struct in_addr),
			AF_INET);
		if (hp) {
			if ((cp = strchr(hp->h_name, '.')) &&
			    !strcmp(cp + 1, domain))
				*cp = 0;
			cp = hp->h_name;
		}
	}
	if (cp) {
		strncpy(line, cp, sizeof(line) - 1);
		line[sizeof(line) - 1] = '\0';
	} else {
#define C(x)	((x) & 0xff)
		in = ntohl(in);
		snprintf(line, sizeof line, "%u.%u.%u.%u",
		    C(in >> 24), C(in >> 16), C(in >> 8), C(in));
	}
	return (line);
}

/*
 * Return the name of the network whose address is given.
 * The address is assumed to be that of a net or subnet, not a host.
 */
char *
netname(in, mask)
	in_addr_t in, mask;
{
	char *cp = 0;
	static char line[MAXHOSTNAMELEN];
	struct netent *np = 0;
	int mbits;

	in = ntohl(in);
	mask = ntohl(mask);
	if (!nflag && in != INADDR_ANY) {
		if ((np = getnetbyaddr(in, AF_INET)) != NULL)
			cp = np->n_name;
	}
	mbits = mask ? 33 - ffs(mask) : 0;
	if (cp) {
		strncpy(line, cp, sizeof(line) - 1);
		line[sizeof(line) - 1] = '\0';
	} else if (mbits < 9)
		snprintf(line, sizeof line, "%u/%d", C(in >> 24), mbits);
	else if (mbits < 17)
		snprintf(line, sizeof line, "%u.%u/%d",
		    C(in >> 24) , C(in >> 16), mbits);
	else if (mbits < 25)
		snprintf(line, sizeof line, "%u.%u.%u/%d",
		    C(in >> 24), C(in >> 16), C(in >> 8), mbits);
	else
		snprintf(line, sizeof line, "%u.%u.%u.%u/%d", C(in >> 24),
			C(in >> 16), C(in >> 8), C(in), mbits);
	return (line);
}

#ifdef INET6
char *
netname6(sa6, mask)
	struct sockaddr_in6 *sa6;
	struct in6_addr *mask;
{
	static char line[MAXHOSTNAMELEN + 1];
	struct in6_addr net6;
	u_char *p;
	u_char *lim;
	int masklen, final = 0, illegal = 0;
	int i;
	char hbuf[NI_MAXHOST];
#ifdef NI_WITHSCOPEID
	int flag = NI_WITHSCOPEID;
#else
	int flag = 0;
#endif

	net6 = sa6->sin6_addr;
	for (i = 0; i < sizeof(net6); i++)
		net6.s6_addr[i] &= mask->s6_addr[i];
	
	masklen = 0;
	lim = (u_char *)mask + 16;
	for (p = (u_char *)mask; p < lim; p++) {
		if (final && *p) {
			illegal++;
			continue;
		}

		switch (*p & 0xff) {
		case 0xff:
			masklen += 8;
			break;
		case 0xfe:
			masklen += 7;
			final++;
			break;
		case 0xfc:
			masklen += 6;
			final++;
			break;
		case 0xf8:
			masklen += 5;
			final++;
			break;
		case 0xf0:
			masklen += 4;
			final++;
			break;
		case 0xe0:
			masklen += 3;
			final++;
			break;
		case 0xc0:
			masklen += 2;
			final++;
			break;
		case 0x80:
			masklen += 1;
			final++;
			break;
		case 0x00:
			final++;
			break;
		default:
			final++;
			illegal++;
			break;
		}
	}

	if (masklen == 0 && IN6_IS_ADDR_UNSPECIFIED(&sa6->sin6_addr))
		return("default");

	if (illegal)
		fprintf(stderr, "illegal prefixlen\n");

	if (nflag)
		flag |= NI_NUMERICHOST;
	getnameinfo((struct sockaddr *)sa6, sa6->sin6_len, hbuf, sizeof(hbuf),
		    NULL, 0, flag);
	snprintf(line, sizeof(line), "%s/%d", hbuf, masklen);
	return line;
}

char *
routename6(sa6)
	struct sockaddr_in6 *sa6;
{
	static char line[NI_MAXHOST];
#ifdef NI_WITHSCOPEID
	const int niflag = NI_NUMERICHOST | NI_WITHSCOPEID;
#else
	const int niflag = NI_NUMERICHOST;
#endif
	if (getnameinfo((struct sockaddr *)sa6, sa6->sin6_len,
			line, sizeof(line), NULL, 0, niflag) != 0)
		strcpy(line, "");
	return line;
}
#endif /*INET6*/

/*
 * Print routing statistics
 */
void
rt_stats(off)
	u_long off;
{
	struct rtstat rtstat;

	if (off == 0) {
		printf("rtstat: symbol not in namelist\n");
		return;
	}
	kread(off, (char *)&rtstat, sizeof (rtstat));
	printf("routing:\n");
	printf("\t%u bad routing redirect%s\n",
		rtstat.rts_badredirect, plural(rtstat.rts_badredirect));
	printf("\t%u dynamically created route%s\n",
		rtstat.rts_dynamic, plural(rtstat.rts_dynamic));
	printf("\t%u new gateway%s due to redirects\n",
		rtstat.rts_newgateway, plural(rtstat.rts_newgateway));
	printf("\t%u destination%s found unreachable\n",
		rtstat.rts_unreach, plural(rtstat.rts_unreach));
	printf("\t%u use%s of a wildcard route\n",
		rtstat.rts_wildcard, plural(rtstat.rts_wildcard));
}

short ns_nullh[] = {0,0,0};
short ns_bh[] = {-1,-1,-1};

char *
ns_print(sa)
	register struct sockaddr *sa;
{
	register struct sockaddr_ns *sns = (struct sockaddr_ns*)sa;
	struct ns_addr work;
	union { union ns_net net_e; u_long long_e; } net;
	in_port_t port;
	static char mybuf[50], cport[10], chost[25];
	char *host = "";
	register char *p; register u_char *q;

	work = sns->sns_addr;
	port = ntohs(work.x_port);
	work.x_port = 0;
	net.net_e = work.x_net;
	if (ns_nullhost(work) && net.long_e == 0) {
		if (port ) {
			snprintf(mybuf, sizeof mybuf, "*.%xH", port);
			upHex(mybuf);
		} else
			snprintf(mybuf, sizeof mybuf, "*.*");
		return (mybuf);
	}

	if (bcmp(ns_bh, work.x_host.c_host, 6) == 0) {
		host = "any";
	} else if (bcmp(ns_nullh, work.x_host.c_host, 6) == 0) {
		host = "*";
	} else {
		q = work.x_host.c_host;
		snprintf(chost, sizeof chost, "%02x%02x%02x%02x%02x%02xH",
			q[0], q[1], q[2], q[3], q[4], q[5]);
		for (p = chost; *p == '0' && p < chost + 12; p++)
			continue;
		host = p;
	}
	if (port)
		snprintf(cport, sizeof cport, ".%xH", htons(port));
	else
		*cport = 0;

	snprintf(mybuf, sizeof mybuf, "%xH.%s%s", ntohl(net.long_e),
	    host, cport);
	upHex(mybuf);
	return(mybuf);
}

char *
ns_phost(sa)
	struct sockaddr *sa;
{
	register struct sockaddr_ns *sns = (struct sockaddr_ns *)sa;
	struct sockaddr_ns work;
	static union ns_net ns_zeronet;
	char *p;

	work = *sns;
	work.sns_addr.x_port = 0;
	work.sns_addr.x_net = ns_zeronet;

	p = ns_print((struct sockaddr *)&work);
	if (strncmp("0H.", p, 3) == 0) p += 3;
	return(p);
}

u_short ipx_nullh[] = {0,0,0};
u_short ipx_bh[] = {0xffff,0xffff,0xffff};

char *
ipx_print(sa)
	register struct sockaddr *sa;
{
	register struct sockaddr_ipx *sipx = (struct sockaddr_ipx*)sa;
	struct ipx_addr work;
	union { union ipx_net net_e; u_long long_e; } net;
	in_port_t port;
	static char mybuf[50], cport[10], chost[25];
	char *host = "";
	register char *q;

	work = sipx->sipx_addr;
	port = ntohs(work.ipx_port);
	work.ipx_port = 0;
	net.net_e = work.ipx_net;
	if (ipx_nullhost(work) && net.long_e == 0) {
		if (port != 0) {
			snprintf(mybuf, sizeof mybuf, "*.%xH", port);
			upHex(mybuf);
		} else
			snprintf(mybuf, sizeof mybuf, "*.*");
		return (mybuf);
	}

	if (bcmp(ipx_bh, work.ipx_host.c_host, 6) == 0) {
		host = "any";
	} else if (bcmp(ipx_nullh, work.ipx_host.c_host, 6) == 0) {
		host = "*";
	} else {
		q = work.ipx_host.c_host;
		snprintf(chost, sizeof chost, "%02x:%02x:%02x:%02x:%02x:%02x",
		    q[0], q[1], q[2], q[3], q[4], q[5]);
		host = chost;
	}
	if (port)
		snprintf(cport, sizeof cport, ".%xH", htons(port));
	else
		*cport = 0;

	snprintf(mybuf, sizeof mybuf, "%xH.%s%s", ntohl(net.long_e),
	    host, cport);
	upHex(mybuf);
	return(mybuf);
}

char *
ipx_phost(sa)
	struct sockaddr *sa;
{
	register struct sockaddr_ipx *sipx = (struct sockaddr_ipx *)sa;
	struct sockaddr_ipx work;
	static union ipx_net ipx_zeronet;
	char *p;

	work = *sipx;
	work.sipx_addr.ipx_port = 0;
	work.sipx_addr.ipx_net = ipx_zeronet;

	p = ipx_print((struct sockaddr *)&work);
	if (strncmp("0H.", p, 3) == 0) p += 3;
	return(p);
}

static void
encap_print(rt)
	register struct rtentry *rt;
{
	struct sockaddr_encap sen1, sen2, sen3;

	bcopy(kgetsa(rt_key(rt)), &sen1, sizeof(sen1));
	bcopy(kgetsa(rt_mask(rt)), &sen2, sizeof(sen2));
	bcopy(kgetsa(rt->rt_gateway), &sen3, sizeof(sen3));

	printf("%-18s %-5u ", netname(sen1.sen_ip_src.s_addr, 
				      sen2.sen_ip_src.s_addr),
	       sen1.sen_sport);
	
	printf("%-18s %-5u %-5u ", netname(sen1.sen_ip_dst.s_addr, 
					   sen2.sen_ip_dst.s_addr),
	       sen1.sen_dport, sen1.sen_proto);
	
	printf("%s/%08x/%-u\n", inet_ntoa(sen3.sen_ipsp_dst),
	       ntohl(sen3.sen_ipsp_spi), sen3.sen_ipsp_sproto);
}

void
upHex(p0)
	char *p0;
{
	register char *p = p0;
	for (; *p; p++) switch (*p) {

	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
		*p += ('A' - 'a');
	}
}
