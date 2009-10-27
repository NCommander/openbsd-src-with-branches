/*	$OpenBSD: print-tcp.c,v 1.26 2007/10/07 16:41:05 deraadt Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <net/if.h>
#include <net/pfvar.h>

#include <rpc/rpc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif

#include "interface.h"
#include "addrtoname.h"
#include "extract.h"

#include "nfs.h"

static void print_tcp_rst_data(register const u_char *sp, u_int length);

#define MAX_RST_DATA_LEN	30

/* Compatibility */
#ifndef TCPOPT_WSCALE
#define	TCPOPT_WSCALE		3	/* window scale factor (rfc1072) */
#endif
#ifndef TCPOPT_SACKOK
#define	TCPOPT_SACKOK		4	/* selective ack ok (rfc2018) */
#endif
#ifndef TCPOPT_SACK
#define	TCPOPT_SACK		5	/* selective ack (rfc2018) */
#endif
#ifndef TCPOLEN_SACK
#define TCPOLEN_SACK		8	/* length of a SACK block */
#endif
#ifndef TCPOPT_ECHO
#define	TCPOPT_ECHO		6	/* echo (rfc1072) */
#endif
#ifndef TCPOPT_ECHOREPLY
#define	TCPOPT_ECHOREPLY	7	/* echo (rfc1072) */
#endif
#ifndef TCPOPT_TIMESTAMP
#define TCPOPT_TIMESTAMP	8	/* timestamps (rfc1323) */
#endif
#ifndef TCPOPT_CC
#define TCPOPT_CC		11	/* T/TCP CC options (rfc1644) */
#endif
#ifndef TCPOPT_CCNEW
#define TCPOPT_CCNEW		12	/* T/TCP CC options (rfc1644) */
#endif
#ifndef TCPOPT_CCECHO
#define TCPOPT_CCECHO		13	/* T/TCP CC options (rfc1644) */
#endif

/* Definitions required for ECN
   for use if the OS running tcpdump does not have ECN */
#ifndef TH_ECNECHO
#define TH_ECNECHO		0x40	/* ECN Echo in tcp header */
#endif
#ifndef TH_CWR
#define TH_CWR			0x80	/* ECN Cwnd Reduced in tcp header*/
#endif

struct tha {
#ifndef INET6
	struct in_addr src;
	struct in_addr dst;
#else
	struct in6_addr src;
	struct in6_addr dst;
#endif /*INET6*/
	u_int port;
};

struct tcp_seq_hash {
	struct tcp_seq_hash *nxt;
	struct tha addr;
	tcp_seq seq;
	tcp_seq ack;
};

#define TSEQ_HASHSIZE 919

/* These tcp optinos do not have the size octet */
#define ZEROLENOPT(o) ((o) == TCPOPT_EOL || (o) == TCPOPT_NOP)

static struct tcp_seq_hash tcp_seq_hash[TSEQ_HASHSIZE];

#ifndef BGP_PORT
#define BGP_PORT        179
#endif
#define NETBIOS_SSN_PORT 139

static int tcp_cksum(register const struct ip *ip,
		     register const struct tcphdr *tp,
		     register int len)
{
	int i, tlen;
	union phu {
		struct phdr {
			u_int32_t src;
			u_int32_t dst;
			u_char mbz;
			u_char proto;
			u_int16_t len;
		} ph;
		u_int16_t pa[6];
	} phu;
	register const u_int16_t *sp;
	u_int32_t sum;
	tlen = ntohs(ip->ip_len) - ((const char *)tp-(const char*)ip);

	/* pseudo-header.. */
	phu.ph.len = htons(tlen);
	phu.ph.mbz = 0;
	phu.ph.proto = ip->ip_p;
	memcpy(&phu.ph.src, &ip->ip_src.s_addr, sizeof(u_int32_t));
	memcpy(&phu.ph.dst, &ip->ip_dst.s_addr, sizeof(u_int32_t));

	sp = &phu.pa[0];
	sum = sp[0]+sp[1]+sp[2]+sp[3]+sp[4]+sp[5];

	sp = (const u_int16_t *)tp;

	for (i=0; i<(tlen&~1); i+= 2)
		sum += *sp++;

	if (tlen & 1) {
		sum += htons( (*(const char *)sp) << 8);
	}

	while (sum > 0xffff)
		sum = (sum & 0xffff) + (sum >> 16);
	sum = ~sum & 0xffff;

	return (sum);
}


void
tcp_print(register const u_char *bp, register u_int length,
	  register const u_char *bp2)
{
	register const struct tcphdr *tp;
	register const struct ip *ip;
	register u_char flags;
	register int hlen;
	register char ch;
	register struct tcp_seq_hash *th = NULL;
	register int rev = 0;
	u_int16_t sport, dport, win, urp;
	tcp_seq seq, ack;
#ifdef INET6
	register const struct ip6_hdr *ip6;
#endif

	tp = (struct tcphdr *)bp;
	switch (((struct ip *)bp2)->ip_v) {
	case 4:
		ip = (struct ip *)bp2;
#ifdef INET6
		ip6 = NULL;
#endif
		break;
#ifdef INET6
	case 6:
		ip = NULL;
		ip6 = (struct ip6_hdr *)bp2;
		break;
#endif
	default:
		(void)printf("invalid ip version");
		return;
	}

	ch = '\0';
	if (length < sizeof(*tp)) {
		(void)printf("truncated-tcp %d", length);
		return;
	}

	if (!TTEST(tp->th_dport)) {
#ifdef INET6
		if (ip6) {
			(void)printf("%s > %s: [|tcp]",
				ip6addr_string(&ip6->ip6_src),
				ip6addr_string(&ip6->ip6_dst));
		} else
#endif /*INET6*/
		{
			(void)printf("%s > %s: [|tcp]",
				ipaddr_string(&ip->ip_src),
				ipaddr_string(&ip->ip_dst));
		}
		return;
	}

	sport = ntohs(tp->th_sport);
	dport = ntohs(tp->th_dport);

#ifdef INET6
	if (ip6) {
		if (ip6->ip6_nxt == IPPROTO_TCP) {
			(void)printf("%s.%s > %s.%s: ",
				ip6addr_string(&ip6->ip6_src),
				tcpport_string(sport),
				ip6addr_string(&ip6->ip6_dst),
				tcpport_string(dport));
		} else {
			(void)printf("%s > %s: ",
				tcpport_string(sport), tcpport_string(dport));
		}
	} else
#endif /*INET6*/
	{
		if (ip->ip_p == IPPROTO_TCP) {
			(void)printf("%s.%s > %s.%s: ",
				ipaddr_string(&ip->ip_src),
				tcpport_string(sport),
				ipaddr_string(&ip->ip_dst),
				tcpport_string(dport));
		} else {
			(void)printf("%s > %s: ",
				tcpport_string(sport), tcpport_string(dport));
		}
	}

	if (!qflag && TTEST(tp->th_seq) && !TTEST(tp->th_ack))
		(void)printf("%u ", ntohl(tp->th_seq));

	TCHECK(*tp);
	seq = ntohl(tp->th_seq);
	ack = ntohl(tp->th_ack);
	win = ntohs(tp->th_win);
	urp = ntohs(tp->th_urp);
	hlen = tp->th_off * 4;

	if (qflag) {
		(void)printf("tcp %d", length - tp->th_off * 4);
		return;
	} else if (packettype != PT_TCP) {

		/*
		 * If data present and NFS port used, assume NFS.
		 * Pass offset of data plus 4 bytes for RPC TCP msg length
		 * to NFS print routines.
		 */
		u_int len = length - hlen;
		if ((u_char *)tp + 4 + sizeof(struct rpc_msg) <= snapend &&
		    dport == NFS_PORT) {
			nfsreq_print((u_char *)tp + hlen + 4, len,
				     (u_char *)ip);
			return;
		} else if ((u_char *)tp + 4 + 
		    sizeof(struct rpc_msg) <= snapend && sport == NFS_PORT) {
			nfsreply_print((u_char *)tp + hlen + 4, len,
				       (u_char *)ip);
			return;
		}
	}
	if ((flags = tp->th_flags) & (TH_SYN|TH_FIN|TH_RST|TH_PUSH|
				      TH_ECNECHO|TH_CWR)) {
		if (flags & TH_SYN)
			putchar('S');
		if (flags & TH_FIN)
			putchar('F');
		if (flags & TH_RST)
			putchar('R');
		if (flags & TH_PUSH)
			putchar('P');
		if (flags & TH_CWR)
			putchar('W');	/* congestion _W_indow reduced (ECN) */
		if (flags & TH_ECNECHO)
			putchar('E');	/* ecn _E_cho sent (ECN) */
	} else
		putchar('.');

	if (!Sflag && (flags & TH_ACK)) {
		struct tha tha;
		/*
		 * Find (or record) the initial sequence numbers for
		 * this conversation.  (we pick an arbitrary
		 * collating order so there's only one entry for
		 * both directions).
		 */
#ifdef INET6
		bzero(&tha, sizeof(tha));
		rev = 0;
		if (ip6) {
			if (sport > dport) {
				rev = 1;
			} else if (sport == dport) {
			    int i;

			    for (i = 0; i < 4; i++) {
				if (((u_int32_t *)(&ip6->ip6_src))[i] >
				    ((u_int32_t *)(&ip6->ip6_dst))[i]) {
					rev = 1;
					break;
				}
			    }
			}
			if (rev) {
				tha.src = ip6->ip6_dst;
				tha.dst = ip6->ip6_src;
				tha.port = dport << 16 | sport;
			} else {
				tha.dst = ip6->ip6_dst;
				tha.src = ip6->ip6_src;
				tha.port = sport << 16 | dport;
			}
		} else {
			if (sport > dport ||
			    (sport == dport &&
			     ip->ip_src.s_addr > ip->ip_dst.s_addr)) {
				rev = 1;
			}
			if (rev) {
				*(struct in_addr *)&tha.src = ip->ip_dst;
				*(struct in_addr *)&tha.dst = ip->ip_src;
				tha.port = dport << 16 | sport;
			} else {
				*(struct in_addr *)&tha.dst = ip->ip_dst;
				*(struct in_addr *)&tha.src = ip->ip_src;
				tha.port = sport << 16 | dport;
			}
		}
#else
		if (sport < dport ||
		    (sport == dport &&
		     ip->ip_src.s_addr < ip->ip_dst.s_addr)) {
			tha.src = ip->ip_src, tha.dst = ip->ip_dst;
			tha.port = sport << 16 | dport;
			rev = 0;
		} else {
			tha.src = ip->ip_dst, tha.dst = ip->ip_src;
			tha.port = dport << 16 | sport;
			rev = 1;
		}
#endif

		for (th = &tcp_seq_hash[tha.port % TSEQ_HASHSIZE];
		     th->nxt; th = th->nxt)
			if (!memcmp((char *)&tha, (char *)&th->addr,
				  sizeof(th->addr)))
				break;

		if (!th->nxt || flags & TH_SYN) {
			/* didn't find it or new conversation */
			if (th->nxt == NULL) {
				th->nxt = (struct tcp_seq_hash *)
					calloc(1, sizeof(*th));
				if (th->nxt == NULL)
					error("tcp_print: calloc");
			}
			th->addr = tha;
			if (rev)
				th->ack = seq, th->seq = ack - 1;
			else
				th->seq = seq, th->ack = ack - 1;
		} else {
			if (rev)
				seq -= th->ack, ack -= th->seq;
			else
				seq -= th->seq, ack -= th->ack;
		}
	}
	hlen = tp->th_off * 4;
	if (hlen > length) {
		(void)printf(" [bad hdr length]");
		return;
	}

	if (ip && ip->ip_v == 4 && vflag) {
		int sum;
		if (TTEST2(tp->th_sport, length)) {
			sum = tcp_cksum(ip, tp, length);
			if (sum != 0)
				(void)printf(" [bad tcp cksum %x!]", sum);
			else
				(void)printf(" [tcp sum ok]");
		}
	}

	/* OS Fingerprint */
	if (oflag && (flags & (TH_SYN|TH_ACK)) == TH_SYN) {
		struct pf_osfp_enlist *head = NULL;
		struct pf_osfp_entry *fp;
		unsigned long left;
		left = (unsigned long)(snapend - (const u_char *)tp);

		if (left >= hlen)
			head = pf_osfp_fingerprint_hdr(ip, ip6, tp);
		if (head) {
			int prev = 0;
			printf(" (src OS:");
			SLIST_FOREACH(fp, head, fp_entry) {
				if (fp->fp_enflags & PF_OSFP_EXPANDED)
					continue;
				if (prev)
					printf(",");
				printf(" %s", fp->fp_class_nm);
				if (fp->fp_version_nm[0])
					printf(" %s", fp->fp_version_nm);
				if (fp->fp_subtype_nm[0])
					printf(" %s", fp->fp_subtype_nm);
				prev = 1;
			}
			printf(")");
		} else {
			if (left < hlen)
				printf(" (src OS: short-pkt)");
			else
				printf(" (src OS: unknown)");
		}
	}

	length -= hlen;
	if (vflag > 1 || length > 0 || flags & (TH_SYN | TH_FIN | TH_RST))
		(void)printf(" %lu:%lu(%d)", (long) seq, (long) (seq + length),
		    length);
	if (flags & TH_ACK)
		(void)printf(" ack %u", ack);

	(void)printf(" win %d", win);

	if (flags & TH_URG)
		(void)printf(" urg %d", urp);
	/*
	 * Handle any options.
	 */
	if ((hlen -= sizeof(*tp)) > 0) {
		register const u_char *cp;
		register int i, opt, len, datalen;

		cp = (const u_char *)tp + sizeof(*tp);
		putchar(' ');
		ch = '<';
		while (hlen > 0) {
			putchar(ch);
			TCHECK(*cp);
			opt = *cp++;
			if (ZEROLENOPT(opt))
				len = 1;
			else {
				TCHECK(*cp);
				len = *cp++;	/* total including type, len */
				if (len < 2 || len > hlen)
					goto bad;
				--hlen;		/* account for length byte */
			}
			--hlen;			/* account for type byte */
			datalen = 0;

/* Bail if "l" bytes of data are not left or were not captured  */
#define LENCHECK(l) { if ((l) > hlen) goto bad; TCHECK2(*cp, l); }

			switch (opt) {

			case TCPOPT_MAXSEG:
				(void)printf("mss");
				datalen = 2;
				LENCHECK(datalen);
				(void)printf(" %u", EXTRACT_16BITS(cp));

				break;

			case TCPOPT_EOL:
				(void)printf("eol");
				break;

			case TCPOPT_NOP:
				(void)printf("nop");
				break;

			case TCPOPT_WSCALE:
				(void)printf("wscale");
				datalen = 1;
				LENCHECK(datalen);
				(void)printf(" %u", *cp);
				break;

			case TCPOPT_SACKOK:
				(void)printf("sackOK");
				if (len != 2)
					(void)printf("[len %d]", len);
				break;

			case TCPOPT_SACK:
			{
				u_long s, e;

				datalen = len - 2;
				if ((datalen % TCPOLEN_SACK) != 0 ||
				    !(flags & TH_ACK)) {
				         (void)printf("malformed sack ");
					 (void)printf("[len %d] ", datalen);
					 break;
				}
				printf("sack %d ", datalen/TCPOLEN_SACK);
				for (i = 0; i < datalen; i += TCPOLEN_SACK) {
					LENCHECK (i + TCPOLEN_SACK);
					s = EXTRACT_32BITS(cp + i);
					e = EXTRACT_32BITS(cp + i + 4);
					if (!Sflag) {
						if (rev) {
							s -= th->seq;
							e -= th->seq;
						} else {
							s -= th->ack;
							e -= th->ack;
						}
					}
					(void) printf("{%lu:%lu} ", s, e);
				}
				break;
			}
			case TCPOPT_ECHO:
				(void)printf("echo");
				datalen = 4;
				LENCHECK(datalen);
				(void)printf(" %u", EXTRACT_32BITS(cp));
				break;

			case TCPOPT_ECHOREPLY:
				(void)printf("echoreply");
				datalen = 4;
				LENCHECK(datalen);
				(void)printf(" %u", EXTRACT_32BITS(cp));
				break;

			case TCPOPT_TIMESTAMP:
				(void)printf("timestamp");
				datalen = 8;
				LENCHECK(4);
				(void)printf(" %u", EXTRACT_32BITS(cp));
				LENCHECK(datalen);
				(void)printf(" %u", EXTRACT_32BITS(cp + 4));
				break;

			case TCPOPT_CC:
				(void)printf("cc");
				datalen = 4;
				LENCHECK(datalen);
				(void)printf(" %u", EXTRACT_32BITS(cp));
				break;

			case TCPOPT_CCNEW:
				(void)printf("ccnew");
				datalen = 4;
				LENCHECK(datalen);
				(void)printf(" %u", EXTRACT_32BITS(cp));
				break;

			case TCPOPT_CCECHO:
				(void)printf("ccecho");
				datalen = 4;
				LENCHECK(datalen);
				(void)printf(" %u", EXTRACT_32BITS(cp));
				break;

			case TCPOPT_SIGNATURE:
				(void)printf("tcpmd5:");
				datalen = len - 2;
				for (i = 0; i < datalen; ++i) {
					LENCHECK(i+1);
					(void)printf("%02x", cp[i]);
				}
				break;

			default:
				(void)printf("opt-%d:", opt);
				datalen = len - 2;
				for (i = 0; i < datalen; ++i) {
					LENCHECK(i+1);
					(void)printf("%02x", cp[i]);
				}
				break;
			}

			/* Account for data printed */
			cp += datalen;
			hlen -= datalen;

			/* Check specification against observed length */
			++datalen;			/* option octet */
			if (!ZEROLENOPT(opt))
				++datalen;		/* size octet */
			if (datalen != len)
				(void)printf("[len %d]", len);
			ch = ',';
			if (opt == TCPOPT_EOL)
				break;
		}
		putchar('>');
	}

	if (length <= 0)
		return;

	/*
	 * Decode payload if necessary.
	*/
	bp += (tp->th_off * 4);
	if (flags & TH_RST) {
		if (vflag)
			print_tcp_rst_data(bp, length);
	} else {
		if (sport == BGP_PORT || dport == BGP_PORT)
			bgp_print(bp, length);
#if 0
		else if (sport == NETBIOS_SSN_PORT || dport == NETBIOS_SSN_PORT)
			nbt_tcp_print(bp, length);
#endif
	}
	return;
bad:
	fputs("[bad opt]", stdout);
	if (ch != '\0')
		putchar('>');
	return;
trunc:
	fputs("[|tcp]", stdout);
	if (ch != '\0')
		putchar('>');
}


/*
 * RFC1122 says the following on data in RST segments:
 *
 *         4.2.2.12  RST Segment: RFC-793 Section 3.4
 *
 *            A TCP SHOULD allow a received RST segment to include data.
 *
 *            DISCUSSION
 *                 It has been suggested that a RST segment could contain
 *                 ASCII text that encoded and explained the cause of the
 *                 RST.  No standard has yet been established for such
 *                 data.
 *
 */

static void
print_tcp_rst_data(register const u_char *sp, u_int length)
{
	int c;

	if (TTEST2(*sp, length))
		printf(" [RST");
	else
		printf(" [!RST");
	if (length > MAX_RST_DATA_LEN) {
		length = MAX_RST_DATA_LEN;	/* can use -X for longer */
		putchar('+');			/* indicate we truncate */
	}
	putchar(' ');
	while (length-- && sp < snapend) {
		c = *sp++;
		safeputchar(c);
	}
	putchar(']');
}
