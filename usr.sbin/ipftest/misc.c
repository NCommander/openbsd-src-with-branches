/*
 * (C)opyright 1993,1994,1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#include <stdio.h>
#include <assert.h>
#include <string.h>
#if !defined(__SVR4) && !defined(__svr4__)
#include <strings.h>
#else
#include <sys/byteorder.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip_var.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcpip.h>
#include <net/if.h>
#include <netinet/ip_fil.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include "ipf.h"
#include "ipt.h"

#ifndef	lint
static	char	sccsid[] = "@(#)misc.c	1.1 10/15/95 (C) 1995 Darren Reed";
#endif

void	debug(), verbose();

extern	int	opts;


void	printpacket(ip)
struct	ip	*ip;
{
	struct	tcphdr	*tcp;

	tcp = (struct tcphdr *)((char *)ip + (ip->ip_hl << 2));
	printf("ip %d(%d) %d ", ip->ip_len, ip->ip_hl << 2, ip->ip_p);
	if (ip->ip_off & 0x1fff)
		printf("@%d", ip->ip_off << 3);
	(void)printf(" %s", inet_ntoa(ip->ip_src));
	if (!(ip->ip_off & 0x1fff))
		if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP)
			(void)printf(",%d", ntohs(tcp->th_sport));
	(void)printf(" > ");
	(void)printf("%s", inet_ntoa(ip->ip_dst));
	if (!(ip->ip_off & 0x1fff))
		if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP)
			(void)printf(",%d", ntohs(tcp->th_dport));
	putchar('\n');
}


#ifdef	NEED_INET_ATON
u_long	inet_aton(buf)
char *buf;
{
	u_long	n;
	char	*s = (char *)&n;
	int	a, b, c, d;

	if (sscanf(buf, "%d.%d.%d.%d", &a, &b, &c, &d) != 4)
		return -1;

	*s++ = (u_char)a;
	*s++ = (u_char)b;
	*s++ = (u_char)c;
	*s++ = (u_char)d;
	return n;
}
#endif


void	verbose(fmt, p1, p2, p3, p4, p5, p6, p7, p8, p9)
char	*fmt, *p1, *p2, *p3, *p4, *p5, *p6, *p7,*p8,*p9;
{
	if (opts & OPT_VERBOSE)
		printf(fmt, p1, p2, p3, p4, p5, p6, p7, p8, p9);
}


void	debug(fmt, p1, p2, p3, p4, p5, p6, p7, p8, p9)
char	*fmt, *p1, *p2, *p3, *p4, *p5, *p6, *p7,*p8,*p9;
{
	if (opts & OPT_DEBUG)
		printf(fmt, p1, p2, p3, p4, p5, p6, p7, p8, p9);
}
