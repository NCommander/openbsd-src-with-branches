/*
 * resend.c (C) 1995 Darren Reed
 *
 * This was written to test what size TCP fragments would get through
 * various TCP/IP packet filters, as used in IP firewalls.  In certain
 * conditions, enough of the TCP header is missing for unpredictable
 * results unless the filter is aware that this can happen.
 *
 */
#ifndef	lint
static	char	sccsid[] = "@(#)resend.c	1.3 1/11/96 (C)1995 Darren Reed";
#endif
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#ifndef	linux
#include <netinet/ip_var.h>
#include <netinet/tcpip.h>
#include <netinet/if_ether.h>
#endif
#include "ip_compat.h"
#ifdef	linux
#include <linux/sockios.h>
#include "tcpip.h"
#endif
#include "ipt.h"


static	u_char	buf[65536];	/* 1 big packet */


printpacket(ip)
ip_t	*ip;
{
	tcphdr_t *t;
	int i, j;

	t = (tcphdr_t *)((char *)ip + (ip->ip_hl << 2));
	if (ip->ip_tos)
		printf("tos %#x ", ip->ip_tos);
	if (ip->ip_off & 0x3fff)
		printf("frag @%#x ", (ip->ip_off & 0x1fff) << 3);
	printf("len %d id %d ", ip->ip_len, ip->ip_id);
	printf("ttl %d p %d src %s", ip->ip_ttl, ip->ip_p,
		inet_ntoa(ip->ip_src));
	if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP)
		printf(",%d", t->th_sport);
	printf(" dst %s", inet_ntoa(ip->ip_dst));
	if (ip->ip_p == IPPROTO_TCP || ip->ip_p == IPPROTO_UDP)
		printf(",%d", t->th_sport);
	if (ip->ip_p == IPPROTO_TCP) {
		printf(" seq %u:%u flags ", t->th_seq, t->th_ack);
		for (j = 0, i = 1; i < 256; i *= 2, j++)
			if (t->th_flags & i)
				printf("%c", "FSRPAU--"[j]);
	}
	putchar('\n');
}


int	ip_resend(dev, mtu, r, gwip, datain)
char	*dev;
struct	in_addr	gwip;
struct	ipread	*r;
char	*datain;
{
	ether_header_t	*eh;
	char	dhost[6];
	ip_t	*ip;
	int	fd, wfd = initdevice(dev, 0, 5), len, i;

	if (datain)
		fd = (*r->r_open)(datain);
	else
		fd = (*r->r_open)("-");
 
	if (fd < 0)
		exit(-1);

	ip = (struct ip *)buf;
	eh = (ether_header_t *)malloc(sizeof(*eh));

	bzero(&eh->ether_shost, sizeof(eh->ether_shost));
	if (gwip.s_addr && (arp((char *)&gwip, dhost) == -1))
	    {
		perror("arp");
		return -2;
	    }

	while ((i = (*r->r_readip)(buf, sizeof(buf), NULL, NULL)) > 0)
	    {
		len = ntohs(ip->ip_len);
		eh = (ether_header_t *)realloc((char *)eh, sizeof(*eh) + len);
		eh->ether_type = htons((u_short)ETHERTYPE_IP);
		if (!gwip.s_addr) {
			if (arp((char *)&gwip,
				(char *)&eh->ether_dhost) == -1) {
				perror("arp");
				continue;
			}
		} else
			bcopy(dhost, (char *)&eh->ether_dhost, sizeof(dhost));
		bcopy(ip, (char *)(eh + 1), len);
		printpacket(ip);

		if (sendip(wfd, eh, sizeof(*eh) + len) == -1)
		    {
			perror("send_packet");
			break;
		    }
	    }
	(*r->r_close)();
	return 0;
}
