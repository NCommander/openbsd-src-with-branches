/*	$OpenBSD: traceroute.c,v 1.137 2015/02/09 23:00:15 deraadt Exp $	*/
/*	$NetBSD: traceroute.c,v 1.10 1995/05/21 15:50:45 mycroft Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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

/*
 * traceroute host  - trace the route ip packets follow going to "host".
 *
 * Attempt to trace the route an ip packet would follow to some
 * internet host.  We find out intermediate hops by launching probe
 * packets with a small ttl (time to live) then listening for an
 * icmp "time exceeded" reply from a gateway.  We start our probes
 * with a ttl of one and increase by one until we get an icmp "port
 * unreachable" (which means we got to "host") or hit a max (which
 * defaults to 64 hops & can be changed with the -m flag).  Three
 * probes (change with -q flag) are sent at each ttl setting and a
 * line is printed showing the ttl, address of the gateway and
 * round trip time of each probe.  If the probe answers come from
 * different gateways, the address of each responding system will
 * be printed.  If there is no response within a 5 sec. timeout
 * interval (changed with the -w flag), a "*" is printed for that
 * probe.
 *
 * Probe packets are UDP format.  We don't want the destination
 * host to process them so the destination port is set to an
 * unlikely value (if some clod on the destination is using that
 * value, it can be changed with the -p flag).
 *
 * A sample use might be:
 *
 *     [yak 71]% traceroute nis.nsf.net.
 *     traceroute to nis.nsf.net (35.1.1.48), 64 hops max, 56 byte packet
 *      1  helios.ee.lbl.gov (128.3.112.1)  19 ms  19 ms  0 ms
 *      2  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  39 ms  19 ms
 *      3  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  39 ms  19 ms
 *      4  ccngw-ner-cc.Berkeley.EDU (128.32.136.23)  39 ms  40 ms  39 ms
 *      5  ccn-nerif22.Berkeley.EDU (128.32.168.22)  39 ms  39 ms  39 ms
 *      6  128.32.197.4 (128.32.197.4)  40 ms  59 ms  59 ms
 *      7  131.119.2.5 (131.119.2.5)  59 ms  59 ms  59 ms
 *      8  129.140.70.13 (129.140.70.13)  99 ms  99 ms  80 ms
 *      9  129.140.71.6 (129.140.71.6)  139 ms  239 ms  319 ms
 *     10  129.140.81.7 (129.140.81.7)  220 ms  199 ms  199 ms
 *     11  nic.merit.edu (35.1.1.48)  239 ms  239 ms  239 ms
 *
 * Note that lines 2 & 3 are the same.  This is due to a buggy
 * kernel on the 2nd hop system -- lbl-csam.arpa -- that forwards
 * packets with a zero ttl.
 *
 * A more interesting example is:
 *
 *     [yak 72]% traceroute allspice.lcs.mit.edu.
 *     traceroute to allspice.lcs.mit.edu (18.26.0.115), 64 hops max
 *      1  helios.ee.lbl.gov (128.3.112.1)  0 ms  0 ms  0 ms
 *      2  lilac-dmc.Berkeley.EDU (128.32.216.1)  19 ms  19 ms  19 ms
 *      3  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  19 ms  19 ms
 *      4  ccngw-ner-cc.Berkeley.EDU (128.32.136.23)  19 ms  39 ms  39 ms
 *      5  ccn-nerif22.Berkeley.EDU (128.32.168.22)  20 ms  39 ms  39 ms
 *      6  128.32.197.4 (128.32.197.4)  59 ms  119 ms  39 ms
 *      7  131.119.2.5 (131.119.2.5)  59 ms  59 ms  39 ms
 *      8  129.140.70.13 (129.140.70.13)  80 ms  79 ms  99 ms
 *      9  129.140.71.6 (129.140.71.6)  139 ms  139 ms  159 ms
 *     10  129.140.81.7 (129.140.81.7)  199 ms  180 ms  300 ms
 *     11  129.140.72.17 (129.140.72.17)  300 ms  239 ms  239 ms
 *     12  * * *
 *     13  128.121.54.72 (128.121.54.72)  259 ms  499 ms  279 ms
 *     14  * * *
 *     15  * * *
 *     16  * * *
 *     17  * * *
 *     18  ALLSPICE.LCS.MIT.EDU (18.26.0.115)  339 ms  279 ms  279 ms
 *
 * (I start to see why I'm having so much trouble with mail to
 * MIT.)  Note that the gateways 12, 14, 15, 16 & 17 hops away
 * either don't send ICMP "time exceeded" messages or send them
 * with a ttl too small to reach us.  14 - 17 are running the
 * MIT C Gateway code that doesn't send "time exceeded"s.  God
 * only knows what's going on with 12.
 *
 * The silent gateway 12 in the above may be the result of a bug in
 * the 4.[23]BSD network code (and its derivatives):  4.x (x <= 3)
 * sends an unreachable message using whatever ttl remains in the
 * original datagram.  Since, for gateways, the remaining ttl is
 * zero, the icmp "time exceeded" is guaranteed to not make it back
 * to us.  The behavior of this bug is slightly more interesting
 * when it appears on the destination system:
 *
 *      1  helios.ee.lbl.gov (128.3.112.1)  0 ms  0 ms  0 ms
 *      2  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  19 ms  39 ms
 *      3  lilac-dmc.Berkeley.EDU (128.32.216.1)  19 ms  39 ms  19 ms
 *      4  ccngw-ner-cc.Berkeley.EDU (128.32.136.23)  39 ms  40 ms  19 ms
 *      5  ccn-nerif35.Berkeley.EDU (128.32.168.35)  39 ms  39 ms  39 ms
 *      6  csgw.Berkeley.EDU (128.32.133.254)  39 ms  59 ms  39 ms
 *      7  * * *
 *      8  * * *
 *      9  * * *
 *     10  * * *
 *     11  * * *
 *     12  * * *
 *     13  rip.Berkeley.EDU (128.32.131.22)  59 ms !  39 ms !  39 ms !
 *
 * Notice that there are 12 "gateways" (13 is the final
 * destination) and exactly the last half of them are "missing".
 * What's really happening is that rip (a Sun-3 running Sun OS3.5)
 * is using the ttl from our arriving datagram as the ttl in its
 * icmp reply.  So, the reply will time out on the return path
 * (with no notice sent to anyone since icmp's aren't sent for
 * icmp's) until we probe with a ttl that's at least twice the path
 * length.  I.e., rip is really only 7 hops away.  A reply that
 * returns with a ttl of 1 is a clue this problem exists.
 * Traceroute prints a "!" after the time if the ttl is <= 1.
 * Since vendors ship a lot of obsolete (DEC's Ultrix, Sun 3.x) or
 * non-standard (HPUX) software, expect to see this problem
 * frequently and/or take care picking the target host of your
 * probes.
 *
 * Other possible annotations after the time are !H, !N, !P (got a host,
 * network or protocol unreachable, respectively), !S or !F (source
 * route failed or fragmentation needed -- neither of these should
 * ever occur and the associated gateway is busted if you see one).  If
 * almost all the probes result in some kind of unreachable, traceroute
 * will give up and exit.
 *
 * Notes
 * -----
 * This program must be run by root or be setuid.  (I suggest that
 * you *don't* make it setuid -- casual use could result in a lot
 * of unnecessary traffic on our poor, congested nets.)
 *
 * This program requires a kernel mod that does not appear in any
 * system available from Berkeley:  A raw ip socket using proto
 * IPPROTO_RAW must interpret the data sent as an ip datagram (as
 * opposed to data to be wrapped in a ip datagram).  See the README
 * file that came with the source to this program for a description
 * of the mods I made to /sys/netinet/raw_ip.c.  Your mileage may
 * vary.  But, again, ANY 4.x (x < 4) BSD KERNEL WILL HAVE TO BE
 * MODIFIED TO RUN THIS PROGRAM.
 *
 * The udp port usage may appear bizarre (well, ok, it is bizarre).
 * The problem is that an icmp message only contains 8 bytes of
 * data from the original datagram.  8 bytes is the size of a udp
 * header so, if we want to associate replies with the original
 * datagram, the necessary information must be encoded into the
 * udp header (the ip id could be used but there's no way to
 * interlock with the kernel's assignment of ip id's and, anyway,
 * it would have taken a lot more kernel hacking to allow this
 * code to set the ip id).  So, to allow two or more users to
 * use traceroute simultaneously, we use this task's pid as the
 * source port (the high bit is set to move the port number out
 * of the "likely" range).  To keep track of which probe is being
 * replied to (so times and/or hop counts don't get confused by a
 * reply that was delayed in transit), we increment the destination
 * port number before each probe.
 *
 * Don't use this as a coding example.  I was trying to find a
 * routing problem and this code sort-of popped out after 48 hours
 * without sleep.  I was amazed it ever compiled, much less ran.
 *
 * I stole the idea for this program from Steve Deering.  Since
 * the first release, I've learned that had I attended the right
 * IETF working group meetings, I also could have stolen it from Guy
 * Almes or Matt Mathis.  I don't know (or care) who came up with
 * the idea first.  I envy the originators' perspicacity and I'm
 * glad they didn't keep the idea a secret.
 *
 * Tim Seaver, Ken Adelman and C. Philip Wood provided bug fixes and/or
 * enhancements to the original distribution.
 *
 * I've hacked up a round-trip-route version of this that works by
 * sending a loose-source-routed udp datagram through the destination
 * back to yourself.  Unfortunately, SO many gateways botch source
 * routing, the thing is almost worthless.  Maybe one day...
 *
 *  -- Van Jacobson (van@helios.ee.lbl.gov)
 *     Tue Dec 20 03:50:13 PST 1988
 */

#include <sys/time.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet/udp.h>

#define DUMMY_PORT 10010

#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <netmpls/mpls.h>

#include <ctype.h>
#include <err.h>
#include <poll.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#define MAX_LSRR		((MAX_IPOPTLEN - 4) / 4)

#define MPLS_LABEL(m)		((m & MPLS_LABEL_MASK) >> MPLS_LABEL_OFFSET)
#define MPLS_EXP(m)		((m & MPLS_EXP_MASK) >> MPLS_EXP_OFFSET)

/*
 * Format of the data in a (udp) probe packet.
 */
struct packetdata {
	u_char seq;		/* sequence number of this packet */
	u_int8_t ttl;		/* ttl packet left with */
	u_char pad[2];
	u_int32_t sec;		/* time packet left */
	u_int32_t usec;
} __packed;

struct in_addr gateway[MAX_LSRR + 1];
int lsrrlen = 0;
int32_t sec_perturb;
int32_t usec_perturb;

u_char packet[512], *outpacket;	/* last inbound (icmp) packet */

int wait_for_reply(int, struct msghdr *);
void dump_packet(void);
void build_probe4(int, u_int8_t, int);
void build_probe6(int, u_int8_t, int, struct sockaddr *);
void send_probe(int, u_int8_t, int, struct sockaddr *);
struct udphdr *get_udphdr(struct ip6_hdr *, u_char *);
int packet_ok(int, struct msghdr *, int, int, int);
int packet_ok4(struct msghdr *, int, int, int);
int packet_ok6(struct msghdr *, int, int, int);
void icmp_code(int, int, int *, int *);
void icmp4_code(int, int *, int *);
void icmp6_code(int, int *, int *);
void dump_packet(void);
void print_exthdr(u_char *, int);
void check_tos(struct ip*);
void print(struct sockaddr *, int, const char *);
const char *inetname(struct sockaddr*);
void print_asn(struct sockaddr_storage *);
u_short in_cksum(u_short *, int);
char *pr_type(u_int8_t);
int map_tos(char *, int *);
double deltaT(struct timeval *, struct timeval *);
void usage(void);

int rcvsock;			/* receive (icmp) socket file descriptor */
int sndsock;			/* send (udp) socket file descriptor */

struct msghdr rcvmhdr;
struct iovec rcviov[2];

int rcvhlim;
struct in6_pktinfo *rcvpktinfo;

int datalen;			/* How much data */
int headerlen;			/* How long packet's header is */

char *source = 0;
char *hostname;

int nprobes = 3;
u_int8_t max_ttl = IPDEFTTL;
u_int8_t first_ttl = 1;
u_short ident;
u_int16_t srcport;
u_int16_t port = 32768+666;	/* start udp dest port # for probe packets */
u_char	proto = IPPROTO_UDP;
u_int8_t  icmp_type = ICMP_ECHO; /* default ICMP code/type */
#define ICMP_CODE 0;
int options;			/* socket options */
int verbose;
int waittime = 5;		/* time to wait for response (in seconds) */
int nflag;			/* print addresses numerically */
int dump;
int xflag;			/* show ICMP extension header */
int tflag;			/* tos flag was set */
int Aflag;			/* lookup ASN */
int last_tos;
int v6flag;

extern char *__progname;

int
main(int argc, char *argv[])
{
	int mib[4] = { CTL_NET, PF_INET, IPPROTO_IP, IPCTL_DEFTTL };
	int ttl_flag = 0, incflag = 1, protoset = 0, sump = 0;
	int ch, i, lsrr = 0, on = 1, probe, seq = 0, tos = 0, error, packetlen;
	int rcvcmsglen, rcvsock4, rcvsock6, sndsock4, sndsock6;
	int v4sock_errno, v6sock_errno;
	struct addrinfo hints, *res;
	size_t size;
	static u_char *rcvcmsgbuf;
	struct sockaddr_in from4, to4;
	struct sockaddr_in6 from6, to6;
	struct sockaddr *from, *to;
	struct hostent *hp;
	u_int32_t tmprnd;
	struct ip *ip = NULL;
	u_int8_t ttl;
	char *ep, hbuf[NI_MAXHOST], *dest;
	const char *errstr;
	long l;
	uid_t uid;
	u_int rtableid;
	socklen_t len;

	rcvsock4 = rcvsock6 = sndsock4 = sndsock6 = -1;
	v4sock_errno = v6sock_errno = 0;

	if ((rcvsock6 = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6)) < 0)
		v6sock_errno = errno;
	else if ((sndsock6 = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
		v6sock_errno = errno;

	if ((rcvsock4 = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0)
		v4sock_errno = errno;
	else if ((sndsock4 = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
		v4sock_errno = errno;

	/* revoke privs */
	uid = getuid();
	if (setresuid(uid, uid, uid) == -1)
		err(1, "setresuid");

	if (strcmp("traceroute6", __progname) == 0) {
		v6flag = 1;
		if (v6sock_errno != 0)
			errc(5, v6sock_errno, rcvsock6 < 0 ? "socket(ICMPv6)" :
			    "socket(SOCK_DGRAM)");
		rcvsock = rcvsock6;
		sndsock = sndsock6;
		if (rcvsock4 >= 0)
			close(rcvsock4);
		if (sndsock4 >= 0)
			close(sndsock4);
	} else {
		if (v4sock_errno != 0)
			errc(5, v4sock_errno, rcvsock4 < 0 ? "icmp socket" :
			    "raw socket");
		rcvsock = rcvsock4;
		sndsock = sndsock4;
		if (rcvsock6 >= 0)
			close(rcvsock6);
		if (sndsock6 >= 0)
			close(sndsock6);
	}

	if (v6flag) {
		mib[1] = PF_INET6;
		mib[2] = IPPROTO_IPV6;
		mib[3] = IPV6CTL_DEFHLIM;
		/* specify to tell receiving interface */
		if (setsockopt(rcvsock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on,
		    sizeof(on)) < 0)
			err(1, "setsockopt(IPV6_RECVPKTINFO)");

		/* specify to tell hoplimit field of received IP6 hdr */
		if (setsockopt(rcvsock, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &on,
		    sizeof(on)) < 0)
			err(1, "setsockopt(IPV6_RECVHOPLIMIT)");
	}

	size = sizeof(i);
	if (sysctl(mib, sizeof(mib)/sizeof(mib[0]), &i, &size, NULL, 0) == -1)
		err(1, "sysctl");
	max_ttl = i;

	while ((ch = getopt(argc, argv, v6flag ? "AcDdf:Ilm:np:q:Ss:w:vV:" :
	    "AcDdf:g:Ilm:nP:p:q:Ss:t:V:vw:x")) != -1)
		switch (ch) {
		case 'A':
			Aflag = 1;
			break;
		case 'c':
			incflag = 0;
			break;
		case 'd':
			options |= SO_DEBUG;
			break;
		case 'D':
			dump = 1;
			break;
		case 'f':
			first_ttl = strtonum(optarg, 1, max_ttl, &errstr);
			if (errstr)
				errx(1, "min ttl must be 1 to %u.", max_ttl);
			break;
		case 'g':
			if (lsrr >= MAX_LSRR)
				errx(1, "too many gateways; max %d", MAX_LSRR);
			if (inet_aton(optarg, &gateway[lsrr]) == 0) {
				hp = gethostbyname(optarg);
				if (hp == 0)
					errx(1, "unknown host %s", optarg);
				memcpy(&gateway[lsrr], hp->h_addr,
				    hp->h_length);
			}
			if (++lsrr == 1)
				lsrrlen = 4;
			lsrrlen += 4;
			break;
		case 'I':
			if (protoset)
				errx(1, "protocol already set with -P");
			protoset = 1;
			proto = IPPROTO_ICMP;
			break;
		case 'l':
			ttl_flag = 1;
			break;
		case 'm':
			max_ttl = strtonum(optarg, first_ttl, MAXTTL, &errstr);
			if (errstr)
				errx(1, "max ttl must be %u to %u.", first_ttl,
				    MAXTTL);
			break;
		case 'n':
			nflag = 1;
			break;
		case 'p':
			port = strtonum(optarg, 1, 65535, &errstr);
			if (errstr)
				errx(1, "port must be >0, <65536.");
			break;
		case 'P':
			if (protoset)
				errx(1, "protocol already set with -I");
			protoset = 1;
			proto = strtonum(optarg, 1, IPPROTO_MAX - 1, &errstr);
			if (errstr) {
				struct protoent *pent;

				pent = getprotobyname(optarg);
				if (pent)
					proto = pent->p_proto;
				else
					errx(1, "proto must be >=1, or a "
					    "name.");
			}
			break;
		case 'q':
			nprobes = strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr)
				errx(1, "nprobes must be >0.");
			break;
		case 's':
			/*
			 * set the ip source address of the outbound
			 * probe (e.g., on a multi-homed host).
			 */
			source = optarg;
			break;
		case 'S':
			sump = 1;
			break;
		case 't':
			if (!map_tos(optarg, &tos)) {
				if (strlen(optarg) > 1 && optarg[0] == '0' &&
				    optarg[1] == 'x') {
					errno = 0;
					ep = NULL;
					l = strtol(optarg, &ep, 16);
					if (errno || !*optarg || *ep ||
					    l < 0 || l > 255)
						errx(1, "illegal tos value %s",
						    optarg);
					tos = (int)l;
				} else {
					tos = strtonum(optarg, 0, 255, &errstr);
					if (errstr)
						errx(1, "illegal tos value %s",
						    optarg);
				}
			}
			tflag = 1;
			last_tos = tos;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'V':
			rtableid = (unsigned int)strtonum(optarg, 0,
			    RT_TABLEID_MAX, &errstr);
			if (errstr)
				errx(1, "rtable value is %s: %s",
				    errstr, optarg);
			if (setsockopt(sndsock, SOL_SOCKET, SO_RTABLE,
			    &rtableid, sizeof(rtableid)) == -1)
				err(1, "setsockopt SO_RTABLE");
			if (setsockopt(rcvsock, SOL_SOCKET, SO_RTABLE,
			    &rtableid, sizeof(rtableid)) == -1)
				err(1, "setsockopt SO_RTABLE");
			break;
		case 'w':
			waittime = strtonum(optarg, 2, INT_MAX, &errstr);
			if (errstr)
				errx(1, "wait must be >1 sec.");
			break;
		case 'x':
			xflag = 1;
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc < 1 || argc > 2)
		usage();

	setvbuf(stdout, NULL, _IOLBF, 0);

	ident = (getpid() & 0xffff) | 0x8000;
	tmprnd = arc4random();
	sec_perturb = (tmprnd & 0x80000000) ? -(tmprnd & 0x7ff) :
	    (tmprnd & 0x7ff);
	usec_perturb = arc4random();

	(void) memset(&to4, 0, sizeof(to4));
	(void) memset(&to6, 0, sizeof(to6));

	if (inet_aton(*argv, &to4.sin_addr) != 0) {
		hostname = *argv;
		if ((dest = strdup(inet_ntoa(to4.sin_addr))) == NULL)
			errx(1, "malloc");
	} else
		dest = *argv;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = v6flag ? PF_INET6 : PF_INET;
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = 0;
	hints.ai_flags = AI_CANONNAME;
	if ((error = getaddrinfo(dest, NULL, &hints, &res)))
		errx(1, "%s", gai_strerror(error));

	switch (res->ai_family) {
	case AF_INET:
		if (res->ai_addrlen != sizeof(to4))
		    errx(1, "size of sockaddr mismatch");

		to = (struct sockaddr *)&to4;
		from = (struct sockaddr *)&from4;
		break;
	case AF_INET6:
		if (res->ai_addrlen != sizeof(to6))
			errx(1, "size of sockaddr mismatch");

		to = (struct sockaddr *)&to6;
		from = (struct sockaddr *)&from6;
		break;
	default:
		errx(1, "unsupported AF: %d", res->ai_family);
		break;
	}

	memcpy(to, res->ai_addr, res->ai_addrlen);

	if (!hostname) {
		hostname = res->ai_canonname ? strdup(res->ai_canonname) : dest;
		if (!hostname)
			errx(1, "malloc");
	}

	if (res->ai_next) {
		if (getnameinfo(res->ai_addr, res->ai_addrlen, hbuf,
		    sizeof(hbuf), NULL, 0, NI_NUMERICHOST) != 0)
			strlcpy(hbuf, "?", sizeof(hbuf));
		warnx("Warning: %s has multiple "
		    "addresses; using %s\n", hostname, hbuf);
	}
	freeaddrinfo(res);

	if (*++argv) {
		datalen = strtonum(*argv, 0, INT_MAX, &errstr);
		if (errstr)
			errx(1, "datalen out of range");
	}

	switch (to->sa_family) {
	case AF_INET:
		switch (proto) {
		case IPPROTO_UDP:
			headerlen = (sizeof(struct ip) + lsrrlen +
			    sizeof(struct udphdr) + sizeof(struct packetdata));
			break;
		case IPPROTO_ICMP:
			headerlen = (sizeof(struct ip) + lsrrlen +
			    sizeof(struct icmp) + sizeof(struct packetdata));
			break;
		default:
			headerlen = (sizeof(struct ip) + lsrrlen +
			    sizeof(struct packetdata));
		}

		if (datalen < 0 || datalen > IP_MAXPACKET - headerlen)
			errx(1, "packet size must be 0 to %d.",
			    IP_MAXPACKET - headerlen);

		datalen += headerlen;

		if ((outpacket = calloc(1, datalen)) == NULL)
			err(1, "calloc");

		rcviov[0].iov_base = (caddr_t)packet;
		rcviov[0].iov_len = sizeof(packet);
		rcvmhdr.msg_name = (caddr_t)&from4;
		rcvmhdr.msg_namelen = sizeof(from4);
		rcvmhdr.msg_iov = rcviov;
		rcvmhdr.msg_iovlen = 1;
		rcvmhdr.msg_control = NULL;
		rcvmhdr.msg_controllen = 0;

		ip = (struct ip *)outpacket;
		if (lsrr != 0) {
			u_char *p = (u_char *)(ip + 1);

			*p++ = IPOPT_NOP;
			*p++ = IPOPT_LSRR;
			*p++ = lsrrlen - 1;
			*p++ = IPOPT_MINOFF;
			gateway[lsrr] = to4.sin_addr;
			for (i = 1; i <= lsrr; i++) {
				memcpy(p, &gateway[i], sizeof(struct in_addr));
				p += sizeof(struct in_addr);
			}
			ip->ip_dst = gateway[0];
		} else
			ip->ip_dst = to4.sin_addr;
		ip->ip_off = htons(0);
		ip->ip_hl = (sizeof(struct ip) + lsrrlen) >> 2;
		ip->ip_p = proto;
		ip->ip_v = IPVERSION;
		ip->ip_tos = tos;

		if (setsockopt(sndsock, IPPROTO_IP, IP_HDRINCL, (char *)&on,
		    sizeof(on)) < 0)
			err(6, "IP_HDRINCL");

		if (source) {
			(void) memset(&from4, 0, sizeof(from4));
			from4.sin_family = AF_INET;
			if (inet_aton(source, &from4.sin_addr) == 0)
				errx(1, "unknown host %s", source);
			ip->ip_src = from4.sin_addr;
			if (getuid() != 0 &&
			    (ntohl(from4.sin_addr.s_addr) & 0xff000000U) ==
			    0x7f000000U && (ntohl(to4.sin_addr.s_addr) &
			    0xff000000U) != 0x7f000000U)
				errx(1, "source is on 127/8, destination is"
				    " not");
			if (getuid() && bind(sndsock, (struct sockaddr *)&from4,
			    sizeof(from4)) < 0)
				err(1, "bind");
		}
		packetlen = datalen;
		break;
	case AF_INET6:
		/*
		 * packetlen is the size of the complete IP packet sent and
		 * reported in the first line of output.
		 * For IPv4 this is equal to datalen since we are constructing
		 * a raw packet.
		 * For IPv6 we need to always add the size of the IP6 header
		 * and for UDP packets the size of the UDP header since they
		 * are prepended to the packet by the kernel
		 */
		packetlen = sizeof(struct ip6_hdr);
		switch (proto) {
		case IPPROTO_UDP:
			headerlen = sizeof(struct packetdata);
			packetlen += sizeof(struct udphdr);
			break;
		case IPPROTO_ICMP:
			headerlen = sizeof(struct icmp6_hdr) +
			    sizeof(struct packetdata);
			break;
		default:
			errx(1, "Unsupported proto: %hhu", proto);
			break;
		}

		if (datalen < 0 || datalen > IP_MAXPACKET - headerlen)
			errx(1, "packet size must be 0 to %d.",
			    IP_MAXPACKET - headerlen);

		datalen += headerlen;
		packetlen += datalen;

		if ((outpacket = calloc(1, datalen)) == NULL)
			err(1, "calloc");

		/* initialize msghdr for receiving packets */
		rcviov[0].iov_base = (caddr_t)packet;
		rcviov[0].iov_len = sizeof(packet);
		rcvmhdr.msg_name = (caddr_t)&from6;
		rcvmhdr.msg_namelen = sizeof(from6);
		rcvmhdr.msg_iov = rcviov;
		rcvmhdr.msg_iovlen = 1;
		rcvcmsglen = CMSG_SPACE(sizeof(struct in6_pktinfo)) +
		    CMSG_SPACE(sizeof(int));

		if ((rcvcmsgbuf = malloc(rcvcmsglen)) == NULL)
			errx(1, "malloc");
		rcvmhdr.msg_control = (caddr_t) rcvcmsgbuf;
		rcvmhdr.msg_controllen = rcvcmsglen;

		/*
		 * Send UDP or ICMP
		 */
		if (proto == IPPROTO_ICMP) {
			close(sndsock);
			sndsock = rcvsock;
		}

		/*
		 * Source selection
		 */
		memset(&from6, 0, sizeof(from6));
		if (source) {
			memset(&hints, 0, sizeof(hints));
			hints.ai_family = AF_INET6;
			hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
			hints.ai_flags = AI_NUMERICHOST;
			if ((error = getaddrinfo(source, "0", &hints, &res)))
				errx(1, "%s: %s", source, gai_strerror(error));
			if (res->ai_addrlen != sizeof(from6))
				errx(1, "size of sockaddr mismatch");
			memcpy(&from6, res->ai_addr, res->ai_addrlen);
			freeaddrinfo(res);
		} else {
			struct sockaddr_in6 nxt;
			int dummy;

			nxt = to6;
			nxt.sin6_port = htons(DUMMY_PORT);
			if ((dummy = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
				err(1, "socket");
			if (connect(dummy, (struct sockaddr *)&nxt,
			    nxt.sin6_len) < 0)
				err(1, "connect");
			len = sizeof(from6);
			if (getsockname(dummy, (struct sockaddr *)&from6,
			    &len) < 0)
				err(1, "getsockname");
			close(dummy);
		}

		from6.sin6_port = htons(0);
		if (bind(sndsock, (struct sockaddr *)&from6, from6.sin6_len) <
		    0)
			err(1, "bind sndsock");

		len = sizeof(from6);
		if (getsockname(sndsock, (struct sockaddr *)&from6, &len) < 0)
			err(1, "getsockname");
		srcport = ntohs(from6.sin6_port);
		break;
	default:
		errx(1, "unsupported AF: %d", to->sa_family);
		break;
	}

	if (options & SO_DEBUG) {
		(void) setsockopt(rcvsock, SOL_SOCKET, SO_DEBUG,
		    (char *)&on, sizeof(on));
		(void) setsockopt(sndsock, SOL_SOCKET, SO_DEBUG,
		    (char *)&on, sizeof(on));
	}

	if (setsockopt(sndsock, SOL_SOCKET, SO_SNDBUF, (char *)&datalen,
	    sizeof(datalen)) < 0)
		err(6, "SO_SNDBUF");

	if (getnameinfo(to, to->sa_len, hbuf,
	    sizeof(hbuf), NULL, 0, NI_NUMERICHOST))
		strlcpy(hbuf, "(invalid)", sizeof(hbuf));
	fprintf(stderr, "%s to %s (%s)", __progname, hostname, hbuf);
	if (source)
		fprintf(stderr, " from %s", source);
	fprintf(stderr, ", %u hops max, %d byte packets\n", max_ttl, packetlen);
	(void) fflush(stderr);

	if (first_ttl > 1)
		printf("Skipping %u intermediate hops\n", first_ttl - 1);

	for (ttl = first_ttl; ttl && ttl <= max_ttl; ++ttl) {
		int got_there = 0, unreachable = 0, timeout = 0, loss;
		in_addr_t lastaddr = 0;
		struct in6_addr lastaddr6;

		printf("%2u ", ttl);
		memset(&lastaddr6, 0, sizeof(lastaddr6));
		for (probe = 0, loss = 0; probe < nprobes; ++probe) {
			int cc;
			struct timeval t1, t2;

			(void) gettimeofday(&t1, NULL);
			send_probe(++seq, ttl, incflag, to);
			while ((cc = wait_for_reply(rcvsock, &rcvmhdr))) {
				(void) gettimeofday(&t2, NULL);
				i = packet_ok(to->sa_family, &rcvmhdr, cc, seq,
				    incflag);
				/* Skip short packet */
				if (i == 0)
					continue;
				if (to->sa_family == AF_INET) {
					ip = (struct ip *)packet;
					if (from4.sin_addr.s_addr != lastaddr) {
						print(from,
						    cc - (ip->ip_hl << 2),
						    inet_ntop(AF_INET,
						    &ip->ip_dst, hbuf,
						    sizeof(hbuf)));
						lastaddr =
						    from4.sin_addr.s_addr;
					}
				} else if (to->sa_family == AF_INET6) {
					if (!IN6_ARE_ADDR_EQUAL(
					    &from6.sin6_addr, &lastaddr6)) {
						print(from, cc, rcvpktinfo ?
						    inet_ntop( AF_INET6,
						    &rcvpktinfo->ipi6_addr,
						    hbuf, sizeof(hbuf)) : "?");
						lastaddr6 = from6.sin6_addr;
					}
				} else
					errx(1, "unsupported AF: %d",
					    to->sa_family);

				printf("  %g ms", deltaT(&t1, &t2));
				if (ttl_flag)
					printf(" (%u)", v6flag ? rcvhlim :
					    ip->ip_ttl);
				if (to->sa_family == AF_INET) {
					if (i == -2) {
						if (ip->ip_ttl <= 1)
							printf(" !");
						++got_there;
						break;
					}

					if (tflag)
						check_tos(ip);
				}

				/* time exceeded in transit */
				if (i == -1)
					break;
				icmp_code(to->sa_family, i - 1, &got_there,
				    &unreachable);
				break;
			}
			if (cc == 0) {
				printf(" *");
				timeout++;
				loss++;
			} else if (cc && probe == nprobes - 1 &&
			    (xflag || verbose))
				print_exthdr(packet, cc);
			(void) fflush(stdout);
		}
		if (sump)
			printf(" (%d%% loss)", (loss * 100) / nprobes);
		putchar('\n');
		if (got_there ||
		    (unreachable && (unreachable + timeout) >= nprobes))
			break;
	}
	exit(0);
}

void
print_exthdr(u_char *buf, int cc)
{
	struct icmp_ext_hdr exthdr;
	struct icmp_ext_obj_hdr objhdr;
	struct ip *ip;
	struct icmp *icp;
	int hlen, first;
	u_int32_t label;
	u_int16_t off, olen;
	u_int8_t type;

	ip = (struct ip *)buf;
	hlen = ip->ip_hl << 2;
	if (cc < hlen + ICMP_MINLEN)
		return;
	icp = (struct icmp *)(buf + hlen);
	cc -= hlen + ICMP_MINLEN;
	buf += hlen + ICMP_MINLEN;

	type = icp->icmp_type;
	if (type != ICMP_TIMXCEED && type != ICMP_UNREACH &&
	    type != ICMP_PARAMPROB)
		/* Wrong ICMP type for extension */
		return;

	off = icp->icmp_length * sizeof(u_int32_t);
	if (off == 0)
		/*
		 * rfc 4884 Section 5.5: traceroute MUST try to parse
		 * broken ext headers. Again IETF bent over to please
		 * idotic corporations.
		 */
		off = ICMP_EXT_OFFSET;
	else if (off < ICMP_EXT_OFFSET)
		/* rfc 4884 requires an offset of at least 128 bytes */
		return;

	/* make sure that at least one extension is present */
	if (cc < off + sizeof(exthdr) + sizeof(objhdr))
		/* Not enough space for ICMP extensions */
		return;

	cc -= off;
	buf += off;
	memcpy(&exthdr, buf, sizeof(exthdr));

	/* verify version */
	if ((exthdr.ieh_version & ICMP_EXT_HDR_VMASK) != ICMP_EXT_HDR_VERSION)
		return;

	/* verify checksum */
	if (exthdr.ieh_cksum && in_cksum((u_short *)buf, cc))
		return;

	buf += sizeof(exthdr);
	cc -= sizeof(exthdr);

	while (cc > sizeof(objhdr)) {
		memcpy(&objhdr, buf, sizeof(objhdr));
		olen = ntohs(objhdr.ieo_length);

		/* Sanity check the length field */
		if (olen < sizeof(objhdr) || olen > cc)
			return;

		cc -= olen;

		/* Move past the object header */
		buf += sizeof(objhdr);
		olen -= sizeof(objhdr);

		switch (objhdr.ieo_cnum) {
		case ICMP_EXT_MPLS:
			/* RFC 4950: ICMP Extensions for MPLS */
			switch (objhdr.ieo_ctype) {
			case 1:
				first = 0;
				while (olen >= sizeof(u_int32_t)) {
					memcpy(&label, buf, sizeof(u_int32_t));
					label = htonl(label);
					buf += sizeof(u_int32_t);
					olen -= sizeof(u_int32_t);

					if (first == 0) {
						printf(" [MPLS Label ");
						first++;
					} else
						printf(", ");
					printf("%d", MPLS_LABEL(label));
					if (MPLS_EXP(label))
						printf(" (Exp %x)",
						    MPLS_EXP(label));
				}
				if (olen > 0) {
					printf("|]");
					return;
				}
				if (first != 0)
					printf("]");
				break;
			default:
				buf += olen;
				break;
			}
			break;
		case ICMP_EXT_IFINFO:
		default:
			buf += olen;
			break;
		}
	}
}

void
check_tos(struct ip *ip)
{
	struct icmp *icp;
	struct ip *inner_ip;

	icp = (struct icmp *) (((u_char *)ip)+(ip->ip_hl<<2));
	inner_ip = (struct ip *) (((u_char *)icp)+8);

	if (inner_ip->ip_tos != last_tos)
		printf (" (TOS=%d!)", inner_ip->ip_tos);

	last_tos = inner_ip->ip_tos;
}

int
wait_for_reply(int sock, struct msghdr *mhdr)
{
	struct pollfd pfd[1];
	int cc = 0;

	pfd[0].fd = sock;
	pfd[0].events = POLLIN;
	pfd[0].revents = 0;

	if (poll(pfd, 1, waittime * 1000) > 0)
		cc = recvmsg(rcvsock, mhdr, 0);

	return (cc);
}

void
dump_packet(void)
{
	u_char *p;
	int i;

	fprintf(stderr, "packet data:");
	for (p = outpacket, i = 0; i < datalen; i++) {
		if ((i % 24) == 0)
			fprintf(stderr, "\n ");
		fprintf(stderr, " %02x", *p++);
	}
	fprintf(stderr, "\n");
}

void
build_probe4(int seq, u_int8_t ttl, int iflag)
{
	struct ip *ip = (struct ip *)outpacket;
	u_char *p = (u_char *)(ip + 1);
	struct udphdr *up = (struct udphdr *)(p + lsrrlen);
	struct icmp *icmpp = (struct icmp *)(p + lsrrlen);
	struct packetdata *op;
	struct timeval tv;

	ip->ip_len = htons(datalen);
	ip->ip_ttl = ttl;
	ip->ip_id = htons(ident+seq);

	switch (proto) {
	case IPPROTO_ICMP:
		icmpp->icmp_type = icmp_type;
		icmpp->icmp_code = ICMP_CODE;
		icmpp->icmp_seq = htons(seq);
		icmpp->icmp_id = htons(ident);
		op = (struct packetdata *)(icmpp + 1);
		break;
	case IPPROTO_UDP:
		up->uh_sport = htons(ident);
		if (iflag)
			up->uh_dport = htons(port+seq);
		else
			up->uh_dport = htons(port);
		up->uh_ulen = htons((u_short)(datalen - sizeof(struct ip) -
		    lsrrlen));
		up->uh_sum = 0;
		op = (struct packetdata *)(up + 1);
		break;
	default:
		op = (struct packetdata *)(ip + 1);
		break;
	}
	op->seq = seq;
	op->ttl = ttl;
	(void) gettimeofday(&tv, NULL);

	/*
	 * We don't want hostiles snooping the net to get any useful
	 * information about us. Send the timestamp in network byte order,
	 * and perturb the timestamp enough that they won't know our
	 * real clock ticker. We don't want to perturb the time by too
	 * much: being off by a suspiciously large amount might indicate
	 * OpenBSD.
	 *
	 * The timestamps in the packet are currently unused. If future
	 * work wants to use them they will have to subtract out the
	 * perturbation first.
	 */
	(void) gettimeofday(&tv, NULL);
	op->sec = htonl(tv.tv_sec + sec_perturb);
	op->usec = htonl((tv.tv_usec + usec_perturb) % 1000000);

	if (proto == IPPROTO_ICMP && icmp_type == ICMP_ECHO) {
		icmpp->icmp_cksum = 0;
		icmpp->icmp_cksum = in_cksum((u_short *)icmpp,
		    datalen - sizeof(struct ip) - lsrrlen);
		if (icmpp->icmp_cksum == 0)
			icmpp->icmp_cksum = 0xffff;
	}
}

void
build_probe6(int seq, u_int8_t hops, int iflag, struct sockaddr *to)
{
	struct timeval tv;
	struct packetdata *op;
	int i;

	i = hops;
	if (setsockopt(sndsock, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
	    (char *)&i, sizeof(i)) < 0)
		warn("setsockopt IPV6_UNICAST_HOPS");

	if (iflag)
		((struct sockaddr_in6*)to)->sin6_port = htons(port + seq);
	else
		((struct sockaddr_in6*)to)->sin6_port = htons(port);
	(void) gettimeofday(&tv, NULL);

	if (proto == IPPROTO_ICMP) {
		struct icmp6_hdr *icp = (struct icmp6_hdr *)outpacket;

		icp->icmp6_type = ICMP6_ECHO_REQUEST;
		icp->icmp6_code = 0;
		icp->icmp6_cksum = 0;
		icp->icmp6_id = ident;
		icp->icmp6_seq = htons(seq);
		op = (struct packetdata *)(outpacket +
		    sizeof(struct icmp6_hdr));
	} else
		op = (struct packetdata *)outpacket;
	op->seq = seq;
	op->ttl = hops;
	op->sec = htonl(tv.tv_sec);
	op->usec = htonl(tv.tv_usec);
}

void
send_probe(int seq, u_int8_t ttl, int iflag, struct sockaddr *to)
{
	int i;

	switch (to->sa_family) {
	case AF_INET:
		build_probe4(seq, ttl, iflag);
		break;
	case AF_INET6:
		build_probe6(seq, ttl, iflag, to);
		break;
	default:
		errx(1, "unsupported AF: %d", to->sa_family);
		break;
	}

	if (dump)
		dump_packet();

	i = sendto(sndsock, outpacket, datalen, 0, to, to->sa_len);
	if (i < 0 || i != datalen)  {
		if (i < 0)
			warn("sendto");
		printf("%s: wrote %s %d chars, ret=%d\n", __progname, hostname,
		    datalen, i);
		(void) fflush(stdout);
	}
}

double
deltaT(struct timeval *t1p, struct timeval *t2p)
{
	double dt;

	dt = (double)(t2p->tv_sec - t1p->tv_sec) * 1000.0 +
	    (double)(t2p->tv_usec - t1p->tv_usec) / 1000.0;
	return (dt);
}

static char *ttab[] = {
	"Echo Reply",
	"ICMP 1",
	"ICMP 2",
	"Dest Unreachable",
	"Source Quench",
	"Redirect",
	"ICMP 6",
	"ICMP 7",
	"Echo",
	"Router Advert",
	"Router Solicit",
	"Time Exceeded",
	"Param Problem",
	"Timestamp",
	"Timestamp Reply",
	"Info Request",
	"Info Reply",
	"Mask Request",
	"Mask Reply"
};

/*
 * Convert an ICMP "type" field to a printable string.
 */
char *
pr_type(u_int8_t t)
{
	if (t > 18)
		return ("OUT-OF-RANGE");
	return (ttab[t]);
}

int
packet_ok(int af, struct msghdr *mhdr, int cc, int seq, int iflag)
{
	switch (af) {
	case AF_INET:
		return packet_ok4(mhdr, cc, seq, iflag);
		break;
	case AF_INET6:
		return packet_ok6(mhdr, cc, seq, iflag);
		break;
	default:
		errx(1, "unsupported AF: %d", af);
		break;
	}
}

int
packet_ok4(struct msghdr *mhdr, int cc,int seq, int iflag)
{
	struct sockaddr_in *from = (struct sockaddr_in *)mhdr->msg_name;
	struct icmp *icp;
	u_char code;
	char *buf = (char *)mhdr->msg_iov[0].iov_base;
	u_int8_t type;
	int hlen;
	struct ip *ip;

	ip = (struct ip *) buf;
	hlen = ip->ip_hl << 2;
	if (cc < hlen + ICMP_MINLEN) {
		if (verbose)
			printf("packet too short (%d bytes) from %s\n", cc,
			    inet_ntoa(from->sin_addr));
		return (0);
	}
	cc -= hlen;
	icp = (struct icmp *)(buf + hlen);
	type = icp->icmp_type;
	code = icp->icmp_code;
	if ((type == ICMP_TIMXCEED && code == ICMP_TIMXCEED_INTRANS) ||
	    type == ICMP_UNREACH || type == ICMP_ECHOREPLY) {
		struct ip *hip;
		struct udphdr *up;
		struct icmp *icmpp;

		hip = &icp->icmp_ip;
		hlen = hip->ip_hl << 2;

		switch (proto) {
		case IPPROTO_ICMP:
			if (icmp_type == ICMP_ECHO &&
			    type == ICMP_ECHOREPLY &&
			    icp->icmp_id == htons(ident) &&
			    icp->icmp_seq == htons(seq))
				return (-2); /* we got there */

			icmpp = (struct icmp *)((u_char *)hip + hlen);
			if (hlen + 8 <= cc && hip->ip_p == IPPROTO_ICMP &&
			    icmpp->icmp_id == htons(ident) &&
			    icmpp->icmp_seq == htons(seq))
				return (type == ICMP_TIMXCEED? -1 : code + 1);
			break;

		case IPPROTO_UDP:
			up = (struct udphdr *)((u_char *)hip + hlen);
			if (hlen + 12 <= cc && hip->ip_p == proto &&
			    up->uh_sport == htons(ident) &&
			    ((iflag && up->uh_dport == htons(port + seq)) ||
			    (!iflag && up->uh_dport == htons(port))))
				return (type == ICMP_TIMXCEED? -1 : code + 1);
			break;
		default:
			/* this is some odd, user specified proto,
			 * how do we check it?
			 */
			if (hip->ip_p == proto)
				return (type == ICMP_TIMXCEED? -1 : code + 1);
		}
	}
	if (verbose) {
		int i;
		in_addr_t *lp = (in_addr_t *)&icp->icmp_ip;

		printf("\n%d bytes from %s", cc, inet_ntoa(from->sin_addr));
		printf(" to %s", inet_ntoa(ip->ip_dst));
		printf(": icmp type %u (%s) code %d\n", type, pr_type(type),
		    icp->icmp_code);
		for (i = 4; i < cc ; i += sizeof(in_addr_t))
			printf("%2d: x%8.8lx\n", i, (unsigned long)*lp++);
	}
	return (0);
}

int
packet_ok6(struct msghdr *mhdr, int cc, int seq, int iflag)
{
	struct icmp6_hdr *icp;
	struct sockaddr_in6 *from = (struct sockaddr_in6 *)mhdr->msg_name;
	u_char type, code;
	char *buf = (char *)mhdr->msg_iov[0].iov_base;
	struct cmsghdr *cm;
	int *hlimp;
	char hbuf[NI_MAXHOST];
	int useicmp = (proto == IPPROTO_ICMP);

	if (cc < sizeof(struct icmp6_hdr)) {
		if (verbose) {
			if (getnameinfo((struct sockaddr *)from, from->sin6_len,
			    hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST) != 0)
				strlcpy(hbuf, "invalid", sizeof(hbuf));
			printf("data too short (%d bytes) from %s\n", cc, hbuf);
		}
		return(0);
	}
	icp = (struct icmp6_hdr *)buf;
	/* get optional information via advanced API */
	rcvpktinfo = NULL;
	hlimp = NULL;
	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(mhdr); cm;
	    cm = (struct cmsghdr *)CMSG_NXTHDR(mhdr, cm)) {
		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_PKTINFO &&
		    cm->cmsg_len ==
		    CMSG_LEN(sizeof(struct in6_pktinfo)))
			rcvpktinfo = (struct in6_pktinfo *)(CMSG_DATA(cm));

		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_HOPLIMIT &&
		    cm->cmsg_len == CMSG_LEN(sizeof(int)))
			hlimp = (int *)CMSG_DATA(cm);
	}
	if (rcvpktinfo == NULL || hlimp == NULL) {
		warnx("failed to get received hop limit or packet info");
		rcvhlim = 0;	/*XXX*/
	} else
		rcvhlim = *hlimp;

	type = icp->icmp6_type;
	code = icp->icmp6_code;
	if ((type == ICMP6_TIME_EXCEEDED && code == ICMP6_TIME_EXCEED_TRANSIT)
	    || type == ICMP6_DST_UNREACH) {
		struct ip6_hdr *hip;
		struct udphdr *up;

		hip = (struct ip6_hdr *)(icp + 1);
		if ((up = get_udphdr(hip, (u_char *)(buf + cc))) == NULL) {
			if (verbose)
				warnx("failed to get upper layer header");
			return(0);
		}
		if (useicmp &&
		    ((struct icmp6_hdr *)up)->icmp6_id == ident &&
		    ((struct icmp6_hdr *)up)->icmp6_seq == htons(seq))
			return (type == ICMP6_TIME_EXCEEDED ? -1 : code + 1);
		else if (!useicmp &&
		    up->uh_sport == htons(srcport) &&
		    ((iflag && up->uh_dport == htons(port + seq)) ||
		    (!iflag && up->uh_dport == htons(port))))
			return (type == ICMP6_TIME_EXCEEDED ? -1 : code + 1);
	} else if (useicmp && type == ICMP6_ECHO_REPLY) {
		if (icp->icmp6_id == ident &&
		    icp->icmp6_seq == htons(seq))
			return (ICMP6_DST_UNREACH_NOPORT + 1);
	}
	if (verbose) {
		char sbuf[NI_MAXHOST], dbuf[INET6_ADDRSTRLEN];
		u_int8_t *p;
		int i;

		if (getnameinfo((struct sockaddr *)from, from->sin6_len,
		    sbuf, sizeof(sbuf), NULL, 0, NI_NUMERICHOST) != 0)
			strlcpy(sbuf, "invalid", sizeof(sbuf));
		printf("\n%d bytes from %s to %s", cc, sbuf,
		    rcvpktinfo ? inet_ntop(AF_INET6, &rcvpktinfo->ipi6_addr,
		    dbuf, sizeof(dbuf)) : "?");
		printf(": icmp type %d (%s) code %d\n", type, pr_type(type),
		    icp->icmp6_code);
		p = (u_int8_t *)(icp + 1);
#define WIDTH	16
		for (i = 0; i < cc; i++) {
			if (i % WIDTH == 0)
				printf("%04x:", i);
			if (i % 4 == 0)
				printf(" ");
			printf("%02x", p[i]);
			if (i % WIDTH == WIDTH - 1)
				printf("\n");
		}
		if (cc % WIDTH != 0)
			printf("\n");
	}
	return(0);
}

void
print(struct sockaddr *from, int cc, const char *to)
{
	char hbuf[NI_MAXHOST];
	if (getnameinfo(from, from->sa_len,
	    hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST) != 0)
		strlcpy(hbuf, "invalid", sizeof(hbuf));
	if (nflag)
		printf(" %s", hbuf);
	else
		printf(" %s (%s)", inetname(from), hbuf);

	if (Aflag)
		print_asn((struct sockaddr_storage *)from);

	if (verbose)
		printf(" %d bytes to %s", cc, to);
}

/*
 * Increment pointer until find the UDP or ICMP header.
 */
struct udphdr *
get_udphdr(struct ip6_hdr *ip6, u_char *lim)
{
	u_char *cp = (u_char *)ip6, nh;
	int hlen;
	int useicmp = (proto == IPPROTO_ICMP);

	if (cp + sizeof(*ip6) >= lim)
		return(NULL);

	nh = ip6->ip6_nxt;
	cp += sizeof(struct ip6_hdr);

	while (lim - cp >= 8) {
		switch (nh) {
		case IPPROTO_ESP:
		case IPPROTO_TCP:
			return(NULL);
		case IPPROTO_ICMPV6:
			return(useicmp ? (struct udphdr *)cp : NULL);
		case IPPROTO_UDP:
			return(useicmp ? NULL : (struct udphdr *)cp);
		case IPPROTO_FRAGMENT:
			hlen = sizeof(struct ip6_frag);
			nh = ((struct ip6_frag *)cp)->ip6f_nxt;
			break;
		case IPPROTO_AH:
			hlen = (((struct ip6_ext *)cp)->ip6e_len + 2) << 2;
			nh = ((struct ip6_ext *)cp)->ip6e_nxt;
			break;
		default:
			hlen = (((struct ip6_ext *)cp)->ip6e_len + 1) << 3;
			nh = ((struct ip6_ext *)cp)->ip6e_nxt;
			break;
		}

		cp += hlen;
	}

	return(NULL);
}

void
icmp_code(int af, int code, int *got_there, int *unreachable)
{
	switch (af) {
	case AF_INET:
		icmp4_code(code, got_there, unreachable);
		break;
	case AF_INET6:
		icmp6_code(code, got_there, unreachable);
		break;
	default:
		errx(1, "unsupported AF: %d", af);
		break;
	}
}

void
icmp4_code(int code, int *got_there, int *unreachable)
{
	struct ip *ip = (struct ip *)packet;

	switch (code) {
	case ICMP_UNREACH_PORT:
		if (ip->ip_ttl <= 1)
			printf(" !");
		++(*got_there);
		break;
	case ICMP_UNREACH_NET:
		++(*unreachable);
		printf(" !N");
		break;
	case ICMP_UNREACH_HOST:
		++(*unreachable);
		printf(" !H");
		break;
	case ICMP_UNREACH_PROTOCOL:
		++(*got_there);
		printf(" !P");
		break;
	case ICMP_UNREACH_NEEDFRAG:
		++(*unreachable);
		printf(" !F");
		break;
	case ICMP_UNREACH_SRCFAIL:
		++(*unreachable);
		printf(" !S");
		break;
	case ICMP_UNREACH_FILTER_PROHIB:
		++(*unreachable);
		printf(" !X");
		break;
	case ICMP_UNREACH_NET_PROHIB: /*misuse*/
		++(*unreachable);
		printf(" !A");
		break;
	case ICMP_UNREACH_HOST_PROHIB:
		++(*unreachable);
		printf(" !C");
		break;
	case ICMP_UNREACH_NET_UNKNOWN:
	case ICMP_UNREACH_HOST_UNKNOWN:
		++(*unreachable);
		printf(" !U");
		break;
	case ICMP_UNREACH_ISOLATED:
		++(*unreachable);
		printf(" !I");
		break;
	case ICMP_UNREACH_TOSNET:
	case ICMP_UNREACH_TOSHOST:
		++(*unreachable);
		printf(" !T");
		break;
	default:
		++(*unreachable);
		printf(" !<%d>", code);
		break;
	}
}

void
icmp6_code(int code, int *got_there, int *unreachable)
{
	switch (code) {
	case ICMP6_DST_UNREACH_NOROUTE:
		++(*unreachable);
		printf(" !N");
		break;
	case ICMP6_DST_UNREACH_ADMIN:
		++(*unreachable);
		printf(" !P");
		break;
	case ICMP6_DST_UNREACH_NOTNEIGHBOR:
		++(*unreachable);
		printf(" !S");
		break;
	case ICMP6_DST_UNREACH_ADDR:
		++(*unreachable);
		printf(" !A");
		break;
	case ICMP6_DST_UNREACH_NOPORT:
		if (rcvhlim >= 0 && rcvhlim <= 1)
			printf(" !");
		++(*got_there);
		break;
	default:
		++(*unreachable);
		printf(" !<%d>", code);
		break;
	}
}

/*
 * Checksum routine for Internet Protocol family headers (C Version)
 */
u_short
in_cksum(u_short *addr, int len)
{
	u_short *w = addr, answer;
	int nleft = len, sum = 0;

	/*
	 *  Our algorithm is simple, using a 32 bit accumulator (sum),
	 *  we add sequential 16 bit words to it, and at the end, fold
	 *  back all the carry bits from the top 16 bits into the lower
	 *  16 bits.
	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1)
		sum += *(u_char *)w;

	/*
	 * add back carry outs from top 16 bits to low 16 bits
	 */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return (answer);
}

/*
 * Construct an Internet address representation.
 */
const char *
inetname(struct sockaddr *sa)
{
	static char line[NI_MAXHOST], domain[HOST_NAME_MAX + 1];
	static int first = 1;
	char *cp;

	if (first) {
		first = 0;
		if (gethostname(domain, sizeof(domain)) == 0 &&
		    (cp = strchr(domain, '.')) != NULL)
			memmove(domain, cp + 1, strlen(cp + 1) + 1);
		else
			domain[0] = 0;
	}
	if (getnameinfo(sa, sa->sa_len, line, sizeof(line), NULL, 0,
	    NI_NAMEREQD) == 0) {
		if ((cp = strchr(line, '.')) != NULL && strcmp(cp + 1,
		    domain) == 0)
			*cp = '\0';
		return (line);
	}

	if (getnameinfo(sa, sa->sa_len, line, sizeof(line), NULL, 0,
	    NI_NUMERICHOST) != 0)
		return ("invalid");
	return (line);
}

void
print_asn(struct sockaddr_storage *ss)
{
	struct rrsetinfo *answers = NULL;
	int counter;
	const u_char *uaddr;
	char qbuf[MAXDNAME];

	switch (ss->ss_family) {
	case AF_INET:
		uaddr = (const u_char *)&((struct sockaddr_in *) ss)->sin_addr;
		if (snprintf(qbuf, sizeof qbuf, "%u.%u.%u.%u."
		    "origin.asn.cymru.com",
		    (uaddr[3] & 0xff), (uaddr[2] & 0xff),
		    (uaddr[1] & 0xff), (uaddr[0] & 0xff)) >= sizeof (qbuf))
			return;
		break;
	case AF_INET6:
		uaddr = (const u_char *)&((struct sockaddr_in6 *) ss)->sin6_addr;
		if (snprintf(qbuf, sizeof qbuf,
		    "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x."
		    "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x."
		    "origin6.asn.cymru.com",
		    (uaddr[15] & 0x0f), ((uaddr[15] >>4)& 0x0f),
		    (uaddr[14] & 0x0f), ((uaddr[14] >>4)& 0x0f),
		    (uaddr[13] & 0x0f), ((uaddr[13] >>4)& 0x0f),
		    (uaddr[12] & 0x0f), ((uaddr[12] >>4)& 0x0f),
		    (uaddr[11] & 0x0f), ((uaddr[11] >>4)& 0x0f),
		    (uaddr[10] & 0x0f), ((uaddr[10] >>4)& 0x0f),
		    (uaddr[9] & 0x0f), ((uaddr[9] >>4)& 0x0f),
		    (uaddr[8] & 0x0f), ((uaddr[8] >>4)& 0x0f),
		    (uaddr[7] & 0x0f), ((uaddr[7] >>4)& 0x0f),
		    (uaddr[6] & 0x0f), ((uaddr[6] >>4)& 0x0f),
		    (uaddr[5] & 0x0f), ((uaddr[5] >>4)& 0x0f),
		    (uaddr[4] & 0x0f), ((uaddr[4] >>4)& 0x0f),
		    (uaddr[3] & 0x0f), ((uaddr[3] >>4)& 0x0f),
		    (uaddr[2] & 0x0f), ((uaddr[2] >>4)& 0x0f),
		    (uaddr[1] & 0x0f), ((uaddr[1] >>4)& 0x0f),
		    (uaddr[0] & 0x0f), ((uaddr[0] >>4)& 0x0f)) >= sizeof (qbuf))
			return;
		break;
	default:
		return;
	}

	if (getrrsetbyname(qbuf, C_IN, T_TXT, 0, &answers) != 0)
		return;
	for (counter = 0; counter < answers->rri_nrdatas; counter++) {
		char *p, *as = answers->rri_rdatas[counter].rdi_data;
		as++; /* skip first byte, it contains length */
		if ((p = strchr(as,'|'))) {
			printf(counter ? ", " : " [");
			p[-1] = 0;
			printf("AS%s", as);
		}
	}
	if (counter)
		printf("]");

	freerrset(answers);
}

int
map_tos(char *s, int *val)
{
	/* DiffServ Codepoints and other TOS mappings */
	const struct toskeywords {
		const char	*keyword;
		int		 val;
	} *t, toskeywords[] = {
		{ "af11",		IPTOS_DSCP_AF11 },
		{ "af12",		IPTOS_DSCP_AF12 },
		{ "af13",		IPTOS_DSCP_AF13 },
		{ "af21",		IPTOS_DSCP_AF21 },
		{ "af22",		IPTOS_DSCP_AF22 },
		{ "af23",		IPTOS_DSCP_AF23 },
		{ "af31",		IPTOS_DSCP_AF31 },
		{ "af32",		IPTOS_DSCP_AF32 },
		{ "af33",		IPTOS_DSCP_AF33 },
		{ "af41",		IPTOS_DSCP_AF41 },
		{ "af42",		IPTOS_DSCP_AF42 },
		{ "af43",		IPTOS_DSCP_AF43 },
		{ "critical",		IPTOS_PREC_CRITIC_ECP },
		{ "cs0",		IPTOS_DSCP_CS0 },
		{ "cs1",		IPTOS_DSCP_CS1 },
		{ "cs2",		IPTOS_DSCP_CS2 },
		{ "cs3",		IPTOS_DSCP_CS3 },
		{ "cs4",		IPTOS_DSCP_CS4 },
		{ "cs5",		IPTOS_DSCP_CS5 },
		{ "cs6",		IPTOS_DSCP_CS6 },
		{ "cs7",		IPTOS_DSCP_CS7 },
		{ "ef",			IPTOS_DSCP_EF },
		{ "inetcontrol",	IPTOS_PREC_INTERNETCONTROL },
		{ "lowdelay",		IPTOS_LOWDELAY },
		{ "netcontrol",		IPTOS_PREC_NETCONTROL },
		{ "reliability",	IPTOS_RELIABILITY },
		{ "throughput",		IPTOS_THROUGHPUT },
		{ NULL,			-1 },
	};

	for (t = toskeywords; t->keyword != NULL; t++) {
		if (strcmp(s, t->keyword) == 0) {
			*val = t->val;
			return (1);
		}
	}

	return (0);
}

void
usage(void)
{
	if (v6flag) {
		fprintf(stderr, "usage: traceroute6 [-AcDdIlnSv] [-f first_hop] "
		    "[-m max_hop] [-p port]\n"
		    "\t[-q nqueries] [-s src_addr] [-V rtable] [-w waittime] "
		    "host\n\t[datalen]\n");
	} else {
		fprintf(stderr,
		    "usage: %s [-AcDdIlnSvx] [-f first_ttl] [-g gateway_addr] "
		    "[-m max_ttl]\n"
		    "\t[-P proto] [-p port] [-q nqueries] [-s src_addr]\n"
		    "\t[-t toskeyword] "
		    "[-V rtable] [-w waittime] host [datalen]\n",
		    __progname);
	}
	exit(1);
}
