/*
 * (C)opyright 1993, 1994, 1995 by Darren Reed.
 *
 * This code may be freely distributed as long as it retains this notice
 * and is not changed in any way.  The author accepts no responsibility
 * for the use of this software.  I hate legaleese, don't you ?
 */
#ifndef	lint
static	char	sccsid[] = "%W% %G% (C)1995 Darren Reed";
#endif
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <netinet/in_systm.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <netinet/if_ether.h>
#include <netinet/ip_var.h>
#include <netinet/tcpip.h>
#include "ip_compat.h"
#ifndef	sun
#if !defined (__OpenBSD__) && !defined (__NetBSD__)
#include "tcpip.h"
#endif
#else
# if defined(__SVR4) || defined(__svr4__)
#include <sys/sysmacros.h>
# endif
#endif

#define	PAUSE()	tv.tv_sec = 0; tv.tv_usec = 10000; \
		  (void) select(0, NULL, NULL, NULL, &tv)

void	ip_test1(dev, mtu, ip, gwip, ptest)
char	*dev;
int	mtu;
ip_t	*ip;
struct	in_addr	gwip;
int	ptest;
{
	struct	timeval	tv;
	udphdr_t *u;
	int	nfd, i, len, id = getpid();

	ip->ip_hl = sizeof(*ip) >> 2;
	ip->ip_v = IPVERSION;
	ip->ip_tos = 0;
	ip->ip_off = 0;
	ip->ip_ttl = 60;
	ip->ip_p = IPPROTO_UDP;
	ip->ip_sum = 0;
	u = (udphdr_t *)(ip + 1);
	u->uh_sport = 1;
	u->uh_dport = 9;
	u->uh_sum = 0;
	u->uh_ulen = sizeof(*u) + 4;
	ip->ip_len = sizeof(*ip) + u->uh_ulen;
	len = ip->ip_len;
	nfd = initdevice(dev, u->uh_sport, 1);

	u->uh_sport = htons(u->uh_sport);
	u->uh_dport = htons(u->uh_dport);
	u->uh_ulen = htons(u->uh_ulen);
	if (!ptest || (ptest == 1)) {
		/*
		 * Part1: hl < len
		 */
		ip->ip_id = 0;
		printf("1.1. sending packets with ip_hl < ip_len\n");
		for (i = 0; i < ((sizeof(*ip) + u->uh_ulen) >> 2); i++) {
			ip->ip_hl = i >> 2;
			(void) send_ip(nfd, 1500, ip, gwip);
			printf("%d\r", i);
			fflush(stdout);
			PAUSE();
		}
		putchar('\n');
	}

	if (!ptest || (ptest == 2)) {
		/*
		 * Part2: hl > len
		 */
		ip->ip_id = 0;
		printf("1.2. sending packets with ip_hl > ip_len\n");
		for (; i < ((sizeof(*ip) * 2 + u->uh_ulen) >> 2); i++) {
			ip->ip_hl = i >> 2;
			(void) send_ip(nfd, 1500, ip, gwip);
			printf("%d\r", i);
			fflush(stdout);
			PAUSE();
		}
		putchar('\n');
	}

	if (!ptest || (ptest == 3)) {
		/*
		 * Part3: v < 4
		 */
		ip->ip_id = 0;
		printf("1.3. ip_v < 4\n");
		ip->ip_hl = sizeof(*ip) >> 2;
		for (i = 0; i < 4; i++) {
			ip->ip_v = i;
			(void) send_ip(nfd, 1500, ip, gwip);
			printf("%d\r", i);
			fflush(stdout);
			PAUSE();
		}
		putchar('\n');
	}

	if (!ptest || (ptest == 4)) {
		/*
		 * Part4: v > 4
		 */
		ip->ip_id = 0;
		printf("1.4. ip_v > 4\n");
		for (i = 5; i < 16; i++) {
			ip->ip_v = i;
			(void) send_ip(nfd, 1500, ip, gwip);
			printf("%d\r", i);
			fflush(stdout);
			PAUSE();
		}
		putchar('\n');
	}

	if (!ptest || (ptest == 5)) {
		/*
		 * Part5: len < packet
		 */
		ip->ip_id = 0;
		ip->ip_v = IPVERSION;
		i = ip->ip_len + 1;
		ip->ip_len = htons(ip->ip_len);
		ip->ip_off = htons(ip->ip_off);
		printf("1.5.0 ip_len < packet size (size++, long packets)\n");
		for (; i < (ip->ip_len * 2); i++) {
			ip->ip_id = htons(id++);
			ip->ip_sum = 0;
			ip->ip_sum = chksum(ip, ip->ip_hl << 2);
			(void) send_ether(nfd, ip, i, gwip);
			printf("%d\r", i);
			fflush(stdout);
			PAUSE();
		}
		putchar('\n');
		printf("1.5.1 ip_len < packet size (ip_len-, short packets)\n");
		for (i = len; i > 0; i--) {
			ip->ip_id = htons(id++);
			ip->ip_len = htons(i);
			ip->ip_sum = 0;
			ip->ip_sum = chksum(ip, ip->ip_hl << 2);
			(void) send_ether(nfd, ip, len, gwip);
			printf("%d\r", i);
			fflush(stdout);
			PAUSE();
		}
		putchar('\n');
	}

	if (!ptest || (ptest == 6)) {
		/*
		 * Part6: len > packet
		 */
		ip->ip_id = 0;
		printf("1.6.0 ip_len > packet size (increase ip_len)\n");
		for (i = len + 1; i < (len * 2); i++) {
			ip->ip_id = htons(id++);
			ip->ip_len = htons(i);
			ip->ip_sum = 0;
			ip->ip_sum = chksum(ip, ip->ip_hl << 2);
			(void) send_ether(nfd, ip, len, gwip);
			printf("%d\r", i);
			fflush(stdout);
			PAUSE();
		}
		putchar('\n');
		ip->ip_len = htons(len);
		printf("1.6.1 ip_len > packet size (size--, short packets)\n");
		for (i = len; i > 0; i--) {
			ip->ip_id = htons(id++);
			ip->ip_sum = 0;
			ip->ip_sum = chksum(ip, ip->ip_hl << 2);
			(void) send_ether(nfd, ip, i, gwip);
			printf("%d\r", i);
			fflush(stdout);
			PAUSE();
		}
		putchar('\n');
	}

	if (!ptest || (ptest == 7)) {
		/*
		 * Part7: 0 length fragment
		 */
		printf("1.7.0 Zero length fragments (ip_off = 0x2000)\n");
		ip->ip_id = 0;
		ip->ip_len = sizeof(*ip);
		ip->ip_off = htons(IP_MF);
		(void) send_ip(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("1.7.1 Zero length fragments (ip_off = 0x3000)\n");
		ip->ip_id = 0;
		ip->ip_len = sizeof(*ip);
		ip->ip_off = htons(IP_MF);
		(void) send_ip(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("1.7.2 Zero length fragments (ip_off = 0xa000)\n");
		ip->ip_id = 0;
		ip->ip_len = sizeof(*ip);
		ip->ip_off = htons(0xa000);
		(void) send_ip(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("1.7.3 Zero length fragments (ip_off = 0x0100)\n");
		ip->ip_id = 0;
		ip->ip_len = sizeof(*ip);
		ip->ip_off = htons(0x0100);
		(void) send_ip(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();
	}

	if (!ptest || (ptest == 8)) {
		struct	timeval	tv;

		gettimeofday(&tv, NULL);
		srand(tv.tv_sec ^ getpid() ^ tv.tv_usec);
		/*
		 * Part8: 63k packet + 1k fragment at offset 0x1ffe
		 */
		ip->ip_off = IP_MF;
		u->uh_dport = htons(9);
		ip->ip_id = htons(id++);
		printf("1.8. 63k packet + 1k fragment at offset 0x1ffe\n");
		ip->ip_len = 768 + 20 + 8;
		if ((rand() & 0x1f) != 0) {
			(void) send_ip(nfd, mtu, ip, gwip);
			printf("%d\r", i);
		} else
			printf("skip 0\n");

		ip->ip_len = MIN(768 + 20, mtu - 68);
		i = 512;
		for (; i < (63 * 1024 + 768); i += 768) {
			ip->ip_off = IP_MF | (i >> 3);
			if ((rand() & 0x1f) != 0) {
				(void) send_ip(nfd, mtu, ip, gwip);
				printf("%d\r", i);
			} else
				printf("skip %d\n", i);
			fflush(stdout);
			PAUSE();
		}
		ip->ip_len = 896 + 20;
		ip->ip_off = IP_MF | (i >> 3);
		if ((rand() & 0x1f) != 0) {
			(void) send_ip(nfd, mtu, ip, gwip);
			printf("%d\r", i);
		} else
			printf("skip\n");
		putchar('\n');
		fflush(stdout);
	}

	ip->ip_len = len;
	ip->ip_off = 0;
	if (!ptest || (ptest == 9)) {
		/*
		 * Part9: off & 0x8000 == 0x8000
		 */
		ip->ip_id = 0;
		ip->ip_off = 0x8000;
		printf("1.9. ip_off & 0x8000 == 0x8000\n");
		(void) send_ip(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();
	}

	ip->ip_off = 0;

	if (!ptest || (ptest == 10)) {
		/*
		 * Part10: ttl = 255
		 */
		ip->ip_id = 0;
		ip->ip_ttl = 255;
		printf("1.10.0 ip_ttl = 255\n");
		(void) send_ip(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		ip->ip_ttl = 128;
		printf("1.10.1 ip_ttl = 128\n");
		(void) send_ip(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		ip->ip_ttl = 0;
		printf("1.10.2 ip_ttl = 0\n");
		(void) send_ip(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();
	}

	(void) close(nfd);
}


void	ip_test2(dev, mtu, ip, gwip, ptest)
char	*dev;
int	mtu;
ip_t	*ip;
struct	in_addr	gwip;
int	ptest;
{
	struct	timeval	tv;
	udphdr_t *u;
	int	nfd, i, len, id = getpid();
	u_char	*s;

	s = (u_char *)(ip + 1);
	nfd = initdevice(dev, 1, 1);

	ip->ip_hl = 6;
	ip->ip_len = ip->ip_hl << 2;
	s[IPOPT_OPTVAL] = IPOPT_NOP;
	s++;
	if (!ptest || (ptest == 1)) {
		/*
		 * Test 1: option length > packet length,
		 *                header length == packet length
		 */
		s[IPOPT_OPTVAL] = IPOPT_TS;
		s[IPOPT_OLEN] = 4;
		s[IPOPT_OFFSET] = IPOPT_MINOFF;
		ip->ip_p = IPPROTO_IP;
		printf("2.1 option length > packet length\n");
		(void) send_ip(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();
	}

	ip->ip_hl = 7;
	ip->ip_len = ip->ip_hl << 2;
	if (!ptest || (ptest == 1)) {
		/*
		 * Test 2: options have length = 0
		 */
		printf("2.2.1 option length = 0, RR\n");
		s[IPOPT_OPTVAL] = IPOPT_RR;
		s[IPOPT_OLEN] = 0;
		(void) send_ip(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("2.2.2 option length = 0, TS\n");
		s[IPOPT_OPTVAL] = IPOPT_TS;
		s[IPOPT_OLEN] = 0;
		(void) send_ip(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("2.2.3 option length = 0, SECURITY\n");
		s[IPOPT_OPTVAL] = IPOPT_SECURITY;
		s[IPOPT_OLEN] = 0;
		(void) send_ip(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("2.2.4 option length = 0, LSRR\n");
		s[IPOPT_OPTVAL] = IPOPT_LSRR;
		s[IPOPT_OLEN] = 0;
		(void) send_ip(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("2.2.5 option length = 0, SATID\n");
		s[IPOPT_OPTVAL] = IPOPT_SATID;
		s[IPOPT_OLEN] = 0;
		(void) send_ip(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("2.2.6 option length = 0, SSRR\n");
		s[IPOPT_OPTVAL] = IPOPT_SSRR;
		s[IPOPT_OLEN] = 0;
		(void) send_ip(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();
	}

	(void) close(nfd);
}


/*
 * test 3 (ICMP)
 */
void	ip_test3(dev, mtu, ip, gwip, ptest)
char	*dev;
int	mtu;
ip_t	*ip;
struct	in_addr	gwip;
int	ptest;
{
	static	int	ict1[10] = { 8, 9, 10, 13, 14, 15, 16, 17, 18, 0 };
	static	int	ict2[8] = { 3, 9, 10, 13, 14, 17, 18, 0 };
	struct	timeval	tv;
	struct	icmp	*icp;
	int	nfd, i, len, id = getpid();
	u_char	*s;

	ip->ip_hl = sizeof(*ip) >> 2;
	ip->ip_v = IPVERSION;
	ip->ip_tos = 0;
	ip->ip_off = 0;
	ip->ip_ttl = 60;
	ip->ip_p = IPPROTO_ICMP;
	ip->ip_sum = 0;
	ip->ip_len = sizeof(*ip) + sizeof(*icp);
	icp = (struct icmp *)((char *)ip + (ip->ip_hl << 2));
	nfd = initdevice(dev, 1, 1);

	if (!ptest || (ptest == 1)) {
		/*
		 * Type 0 - 31, 255, code = 0
		 */
		bzero((char *)icp, sizeof(*icp));
		for (i = 0; i < 32; i++) {
			icp->icmp_type = i;
			(void) send_icmp(nfd, mtu, ip, gwip);
			PAUSE();
			printf("3.1.%d ICMP type %d code 0 (all 0's)\r", i, i);
		}
		icp->icmp_type = 255;
		(void) send_icmp(nfd, mtu, ip, gwip);
		PAUSE();
		printf("3.1.%d ICMP type %d code 0 (all 0's)\r", i, 255);
		putchar('\n');
	}

	if (!ptest || (ptest == 2)) {
		/*
		 * Type 3, code = 0 - 31
		 */
		icp->icmp_type = 3;
		for (i = 0; i < 32; i++) {
			icp->icmp_code = i;
			(void) send_icmp(nfd, mtu, ip, gwip);
			PAUSE();
			printf("3.2.%d ICMP type 3 code %d (all 0's)\r", i, i);
		}
	}

	if (!ptest || (ptest == 3)) {
		/*
		 * Type 4, code = 0,127,128,255
		 */
		icp->icmp_type = 4;
		icp->icmp_code = 0;
		(void) send_icmp(nfd, mtu, ip, gwip);
		PAUSE();
		printf("3.3.1 ICMP type 4 code 0 (all 0's)\r");
		icp->icmp_code = 127;
		(void) send_icmp(nfd, mtu, ip, gwip);
		PAUSE();
		printf("3.3.2 ICMP type 4 code 127 (all 0's)\r");
		icp->icmp_code = 128;
		(void) send_icmp(nfd, mtu, ip, gwip);
		PAUSE();
		printf("3.3.3 ICMP type 4 code 128 (all 0's)\r");
		icp->icmp_code = 255;
		(void) send_icmp(nfd, mtu, ip, gwip);
		PAUSE();
		printf("3.3.4 ICMP type 4 code 255 (all 0's)\r");
	}

	if (!ptest || (ptest == 4)) {
		/*
		 * Type 5, code = 0,127,128,255
		 */
		icp->icmp_type = 5;
		icp->icmp_code = 0;
		(void) send_icmp(nfd, mtu, ip, gwip);
		PAUSE();
		printf("3.4.1 ICMP type 5 code 0 (all 0's)\r");
		icp->icmp_code = 127;
		(void) send_icmp(nfd, mtu, ip, gwip);
		PAUSE();
		printf("3.4.2 ICMP type 5 code 127 (all 0's)\r");
		icp->icmp_code = 128;
		(void) send_icmp(nfd, mtu, ip, gwip);
		PAUSE();
		printf("3.4.3 ICMP type 5 code 128 (all 0's)\r");
		icp->icmp_code = 255;
		(void) send_icmp(nfd, mtu, ip, gwip);
		PAUSE();
		printf("3.4.4 ICMP type 5 code 255 (all 0's)\r");
	}

	if (!ptest || (ptest == 5)) {
		/*
		 * Type 8-10;13-18, code - 0,127,128,255
		 */
		for (i = 0; ict1[i]; i++) {
			icp->icmp_type = ict1[i];
			icp->icmp_code = 0;
			(void) send_icmp(nfd, mtu, ip, gwip);
			PAUSE();
			printf("3.5.%d ICMP type 5 code 0 (all 0's)\r",
				i * 4);
			icp->icmp_code = 127;
			(void) send_icmp(nfd, mtu, ip, gwip);
			PAUSE();
			printf("3.5.%d ICMP type 5 code 127 (all 0's)\r",
				i * 4 + 1);
			icp->icmp_code = 128;
			(void) send_icmp(nfd, mtu, ip, gwip);
			PAUSE();
			printf("3.5.%d ICMP type 5 code 128 (all 0's)\r",
				i * 4 + 2);
			icp->icmp_code = 255;
			(void) send_icmp(nfd, mtu, ip, gwip);
			PAUSE();
			printf("3.5.%d ICMP type 5 code 255 (all 0's)\r",
				i * 4 + 3);
		}
		putchar('\n');
	}

	if (!ptest || (ptest == 6)) {
		/*
		 * Type 12, code - 0,127,128,129,255
		 */
		icp->icmp_type = 12;
		icp->icmp_code = 0;
		(void) send_icmp(nfd, mtu, ip, gwip);
		PAUSE();
		printf("3.6.1 ICMP type 12 code 0 (all 0's)\r");
		icp->icmp_code = 127;
		(void) send_icmp(nfd, mtu, ip, gwip);
		PAUSE();
		printf("3.6.2 ICMP type 12 code 127 (all 0's)\r");
		icp->icmp_code = 128;
		(void) send_icmp(nfd, mtu, ip, gwip);
		PAUSE();
		printf("3.6.3 ICMP type 12 code 128 (all 0's)\r");
		icp->icmp_code = 129;
		(void) send_icmp(nfd, mtu, ip, gwip);
		PAUSE();
		printf("3.6.4 ICMP type 12 code 129 (all 0's)\r");
		icp->icmp_code = 255;
		(void) send_icmp(nfd, mtu, ip, gwip);
		PAUSE();
		printf("3.6.5 ICMP type 12 code 255 (all 0's)\r");
		putchar('\n');
	}

	if (!ptest || (ptest == 7)) {
		/*
		 * Type 3;9-10;13-14;17-18 - shorter packets
		 */
		ip->ip_len = sizeof(*ip) + sizeof(*icp) / 2;
		for (i = 0; ict2[i]; i++) {
			icp->icmp_type = ict1[i];
			icp->icmp_code = 0;
			(void) send_icmp(nfd, mtu, ip, gwip);
			PAUSE();
			printf("3.5.%d ICMP type %d code 0 (all 0's)\r",
				i * 4, icp->icmp_type);
			icp->icmp_code = 127;
			(void) send_icmp(nfd, mtu, ip, gwip);
			PAUSE();
			printf("3.5.%d ICMP type %d code 127 (all 0's)\r",
				i * 4 + 1, icp->icmp_type);
			icp->icmp_code = 128;
			(void) send_icmp(nfd, mtu, ip, gwip);
			PAUSE();
			printf("3.5.%d ICMP type %d code 128 (all 0's)\r",
				i * 4 + 2, icp->icmp_type);
			icp->icmp_code = 255;
			(void) send_icmp(nfd, mtu, ip, gwip);
			PAUSE();
			printf("3.5.%d ICMP type %d code 127 (all 0's)\r",
				i * 4 + 3, icp->icmp_type);
		}
		putchar('\n');
	}
}


/* Perform test 4 (UDP) */

void	ip_test4(dev, mtu, ip, gwip, ptest)
char	*dev;
int	mtu;
ip_t	*ip;
struct	in_addr	gwip;
int	ptest;
{
	struct	timeval	tv;
	struct	udphdr	*u;
	int	nfd, i, len, id = getpid();


	ip->ip_hl = sizeof(*ip) >> 2;
	ip->ip_v = IPVERSION;
	ip->ip_tos = 0;
	ip->ip_off = 0;
	ip->ip_ttl = 60;
	ip->ip_p = IPPROTO_UDP;
	ip->ip_sum = 0;
	u = (udphdr_t *)((char *)ip + (ip->ip_hl << 2));
	u->uh_sport = 1;
	u->uh_dport = 1;
	u->uh_ulen = sizeof(*u) + 4;
	nfd = initdevice(dev, u->uh_sport, 1);

	if (!ptest || (ptest == 1)) {
		/*
		 * Test 1. ulen > packet
		 */
		u->uh_ulen = sizeof(*u) + 4;
		ip->ip_len = (ip->ip_hl << 2) + u->uh_ulen;
		printf("4.1 UDP uh_ulen > packet size - short packets\n");
		for (i = u->uh_ulen * 2; i > sizeof(*u) + 4; i--) {
			u->uh_ulen = i;
			(void) send_udp(nfd, 1500, ip, gwip);
			printf("%d\r", i);
			fflush(stdout);
			PAUSE();
		}
		putchar('\n');
	}

	if (!ptest || (ptest == 2)) {
		/*
		 * Test 2. ulen < packet
		 */
		u->uh_ulen = sizeof(*u) + 4;
		ip->ip_len = (ip->ip_hl << 2) + u->uh_ulen;
		printf("4.2 UDP uh_ulen < packet size - short packets\n");
		for (i = u->uh_ulen * 2; i > sizeof(*u) + 4; i--) {
			ip->ip_len = i;
			(void) send_udp(nfd, 1500, ip, gwip);
			printf("%d\r", i);
			fflush(stdout);
			PAUSE();
		}
		putchar('\n');
	}

	if (!ptest || (ptest == 3)) {
		/*
		 * Test 3: sport = 0, sport = 1, sport = 32767
		 *         sport = 32768, sport = 65535
		 */
		u->uh_ulen = sizeof(*u) + 4;
		ip->ip_len = (ip->ip_hl << 2) + u->uh_ulen;
		printf("4.3.1 UDP sport = 0\n");
		u->uh_sport = 0;
		(void) send_udp(nfd, 1500, ip, gwip);
		printf("0\n");
		fflush(stdout);
		PAUSE();
		printf("4.3.2 UDP sport = 1\n");
		u->uh_sport = 1;
		(void) send_udp(nfd, 1500, ip, gwip);
		printf("1\n");
		fflush(stdout);
		PAUSE();
		printf("4.3.3 UDP sport = 32767\n");
		u->uh_sport = 32767;
		(void) send_udp(nfd, 1500, ip, gwip);
		printf("32767\n");
		fflush(stdout);
		PAUSE();
		printf("4.3.4 UDP sport = 32768\n");
		u->uh_sport = 32768;
		(void) send_udp(nfd, 1500, ip, gwip);
		printf("32768\n");
		putchar('\n');
		fflush(stdout);
		PAUSE();
		printf("4.3.5 UDP sport = 65535\n");
		u->uh_sport = 65535;
		(void) send_udp(nfd, 1500, ip, gwip);
		printf("65535\n");
		fflush(stdout);
		PAUSE();
	}

	if (!ptest || (ptest == 4)) {
		/*
		 * Test 4: dport = 0, dport = 1, dport = 32767
		 *         dport = 32768, dport = 65535
		 */
		u->uh_ulen = sizeof(*u) + 4;
		u->uh_sport = 1;
		ip->ip_len = (ip->ip_hl << 2) + u->uh_ulen;
		printf("4.4.1 UDP dport = 0\n");
		u->uh_dport = 0;
		(void) send_udp(nfd, 1500, ip, gwip);
		printf("0\n");
		fflush(stdout);
		PAUSE();
		printf("4.4.2 UDP dport = 1\n");
		u->uh_dport = 1;
		(void) send_udp(nfd, 1500, ip, gwip);
		printf("1\n");
		fflush(stdout);
		PAUSE();
		printf("4.4.3 UDP dport = 32767\n");
		u->uh_dport = 32767;
		(void) send_udp(nfd, 1500, ip, gwip);
		printf("32767\n");
		fflush(stdout);
		PAUSE();
		printf("4.4.4 UDP dport = 32768\n");
		u->uh_dport = 32768;
		(void) send_udp(nfd, 1500, ip, gwip);
		printf("32768\n");
		fflush(stdout);
		PAUSE();
		printf("4.4.5 UDP dport = 65535\n");
		u->uh_dport = 65535;
		(void) send_udp(nfd, 1500, ip, gwip);
		printf("65535\n");
		fflush(stdout);
		PAUSE();
	}

	if (!ptest || (ptest == 4)) {
		/*
		 * Test 5: sizeof(struct ip) <= MTU <= sizeof(struct udphdr) +
		 * sizeof(struct ip)
		 */
		printf("4.5 UDP 20 <= MTU <= 32\n");
		for (i = sizeof(*ip); i <= u->uh_ulen; i++) {
			(void) send_udp(nfd, i, ip, gwip);
			printf("%d\r", i);
			fflush(stdout);
			PAUSE();
		}
		putchar('\n');
	}
}


/* Perform test 5 (TCP) */

void	ip_test5(dev, mtu, ip, gwip, ptest)
char	*dev;
int	mtu;
ip_t	*ip;
struct	in_addr	gwip;
int	ptest;
{
	struct	timeval	tv;
	tcphdr_t *t;
	int	nfd, i, len, id = getpid();

	t = (tcphdr_t *)((char *)ip + (ip->ip_hl << 2));
	t->th_x2 = 0;
	t->th_off = 0;
	t->th_sport = 1;
	t->th_dport = 1;
	t->th_win = 4096;
	t->th_urp = 0;
	t->th_sum = 0;
	t->th_seq = 1;
	t->th_ack = 0;
	nfd = initdevice(dev, t->th_sport, 1);

	if (!ptest || (ptest == 1)) {
		/*
		 * Test 1: flags variations, 0 - 3f
		 */
		t->th_off = sizeof(*t) >> 2;
		printf("5.1 Test TCP flag combinations\n");
		for (i = 0; i <= (TH_URG|TH_ACK|TH_PUSH|TH_RST|TH_SYN|TH_FIN);
		     i++) {
			t->th_flags = i;
			(void) send_tcp(nfd, mtu, ip, gwip);
			printf("%d\r", i);
			fflush(stdout);
			PAUSE();
		}
		putchar('\n');
	}

	if (!ptest || (ptest == 2)) {
		t->th_flags = TH_SYN;
		/*
		 * Test 2: seq = 0, seq = 1, seq = 0x7fffffff, seq=0x80000000,
		 *         seq = 0xa000000, seq = 0xffffffff
		 */
		printf("5.2.1 TCP seq = 0\n");
		t->th_seq = 0;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("5.2.2 TCP seq = 1\n");
		t->th_seq = 1;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("5.2.3 TCP seq = 0x7fffffff\n");
		t->th_seq = 0x7fffffff;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("5.2.4 TCP seq = 0x80000000\n");
		t->th_seq = 0x80000000;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("5.2.5 TCP seq = 0xc0000000\n");
		t->th_seq = 0xc0000000;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("5.2.6 TCP seq = 0xffffffff\n");
		t->th_seq = 0xffffffff;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();
	}

	if (!ptest || (ptest == 3)) {
		t->th_flags = TH_ACK;
		/*
		 * Test 3: ack = 0, ack = 1, ack = 0x7fffffff, ack = 0x8000000
		 *         ack = 0xa000000, ack = 0xffffffff
		 */
		printf("5.3.1 TCP ack = 0\n");
		t->th_ack = 0;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("5.3.2 TCP ack = 1\n");
		t->th_ack = 1;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("5.3.3 TCP ack = 0x7fffffff\n");
		t->th_ack = 0x7fffffff;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("5.3.4 TCP ack = 0x80000000\n");
		t->th_ack = 0x80000000;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("5.3.5 TCP ack = 0xc0000000\n");
		t->th_ack = 0xc0000000;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("5.3.6 TCP ack = 0xffffffff\n");
		t->th_ack = 0xffffffff;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();
	}

	if (!ptest || (ptest == 4)) {
		t->th_flags = TH_SYN;
		/*
		 * Test 4: win = 0, win = 32768, win = 65535
		 */
		printf("5.4.1 TCP win = 0\n");
		t->th_seq = 0;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("5.4.2 TCP win = 32768\n");
		t->th_seq = 0x7fff;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("5.4.3 TCP win = 65535\n");
		t->th_win = 0xffff;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();
	}

	if (!ptest || (ptest == 5)) {
		t->th_win = 4096;
		/*
		 * Test 5: urp, urp = 0, urp > packetlen, urp < header
		 */
	}

	if (!ptest || (ptest == 6)) {
		/*
		 * Test 6: data offset, off = 0, off is inside, off is outside
		 */
		;
	}

	if (!ptest || (ptest == 7)) {
		t->th_off = 0;
		t->th_flags = TH_SYN;
		/*
		 * Test 7: sport = 0, sport = 1, sport = 32767
		 *         sport = 32768, sport = 65535
		 */
		printf("5.7.1 TCP sport = 0\n");
		t->th_sport = 0;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("5.7.2 TCP sport = 1\n");
		t->th_sport = 1;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("5.7.3 TCP sport = 32767\n");
		t->th_sport = 32767;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("5.7.4 TCP sport = 32768\n");
		t->th_sport = 32768;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("5.7.5 TCP sport = 65535\n");
		t->th_sport = 65535;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();
	}

	if (!ptest || (ptest == 8)) {
		t->th_sport = 1;
		/*
		 * Test 8: dport = 0, dport = 1, dport = 32767
		 *         dport = 32768, dport = 65535
		 */
		printf("5.8.1 TCP dport = 0\n");
		t->th_dport = 0;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("5.8.2 TCP dport = 1\n");
		t->th_dport = 1;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("5.8.3 TCP dport = 32767\n");
		t->th_dport = 32767;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("5.8.4 TCP dport = 32768\n");
		t->th_dport = 32768;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();

		printf("5.8.5 TCP dport = 65535\n");
		t->th_dport = 65535;
		(void) send_tcp(nfd, mtu, ip, gwip);
		fflush(stdout);
		PAUSE();
	}
}
