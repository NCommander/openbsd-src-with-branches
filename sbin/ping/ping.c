/*	$OpenBSD: ping.c,v 1.188 2016/09/17 09:19:44 florian Exp $	*/

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

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Muuss.
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
 * Using the InterNet Control Message Protocol (ICMP) "ECHO" facility,
 * measure round-trip-delays and packet loss across network paths.
 *
 * Author -
 *	Mike Muuss
 *	U. S. Army Ballistic Research Laboratory
 *	December, 1983
 *
 * Status -
 *	Public Domain.  Distribution Unlimited.
 * Bugs -
 *	More statistics could always be gathered.
 *	This program has to run SUID to ROOT to access the ICMP socket.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet/ip_ah.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <siphash.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

struct tv64 {
	u_int64_t	tv64_sec;
	u_int64_t	tv64_nsec;
};

struct payload {
	struct tv64	tv64;
	u_int8_t	mac[SIPHASH_DIGEST_LENGTH];
};

#define	ECHOLEN		8	/* icmp echo header len excluding time */
#define	ECHOTMLEN	sizeof(struct payload)
#define	DEFDATALEN	(64 - ECHOLEN)		/* default data length */
#define	MAXIPLEN	60
#define	MAXICMPLEN	76
#define	MAXPAYLOAD	(IP_MAXPACKET - MAXIPLEN - ECHOLEN)
#define	MAXWAIT_DEFAULT	10			/* secs to wait for response */
#define	NROUTES		9			/* number of record route slots */

#define	A(bit)		rcvd_tbl[(bit)>>3]	/* identify byte in array */
#define	B(bit)		(1 << ((bit) & 0x07))	/* identify bit in byte */
#define	SET(bit)	(A(bit) |= B(bit))
#define	CLR(bit)	(A(bit) &= (~B(bit)))
#define	TST(bit)	(A(bit) & B(bit))

/* various options */
int options;
#define	F_FLOOD		0x0001
#define	F_INTERVAL	0x0002
#define	F_HOSTNAME	0x0004
#define	F_PINGFILLED	0x0008
#define	F_QUIET		0x0010
#define	F_RROUTE	0x0020
#define	F_SO_DEBUG	0x0040
/*			0x0080 */
#define	F_VERBOSE	0x0100
/*			0x0200 */
#define	F_HDRINCL	0x0400
#define	F_TTL		0x0800
/*			0x1000 */
#define	F_AUD_RECV	0x2000
#define	F_AUD_MISS	0x4000

/* multicast options */
int moptions;
#define	MULTICAST_NOLOOP	0x001
#define	MULTICAST_TTL		0x002

#define DUMMY_PORT	10101

/*
 * MAX_DUP_CHK is the number of bits in received table, i.e. the maximum
 * number of received sequence numbers we can keep track of.  Change 128
 * to 8192 for complete accuracy...
 */
#define	MAX_DUP_CHK	(8 * 8192)
int mx_dup_ck = MAX_DUP_CHK;
char rcvd_tbl[MAX_DUP_CHK / 8];

int datalen = DEFDATALEN;
u_char outpackhdr[IP_MAXPACKET+sizeof(struct ip)];
u_char *outpack = outpackhdr+sizeof(struct ip);
char BSPACE = '\b';		/* characters written for flood */
char DOT = '.';
char *hostname;
int ident;			/* process id to identify our packets */

/* counters */
int64_t npackets;		/* max packets to transmit */
int64_t nreceived;		/* # of packets we got back */
int64_t nrepeats;		/* number of duplicates */
int64_t ntransmitted;		/* sequence # for outbound packets = #sent */
int64_t nmissedmax = 1;		/* max value of ntransmitted - nreceived - 1 */
struct timeval interval = {1, 0}; /* interval between packets */

/* timing */
int timing = 0;			/* flag to do timing */
int timinginfo = 0;
unsigned int maxwait = MAXWAIT_DEFAULT;	/* max seconds to wait for response */
double tmin = 999999999.0;	/* minimum round trip time */
double tmax = 0.0;		/* maximum round trip time */
double tsum = 0.0;		/* sum of all times, for doing average */
double tsumsq = 0.0;		/* sum of all times squared, for std. dev. */

struct tv64 tv64_offset;
SIPHASH_KEY mac_key;

struct msghdr smsghdr;
struct iovec smsgiov;

volatile sig_atomic_t seenalrm;
volatile sig_atomic_t seenint;
volatile sig_atomic_t seeninfo;

void			 fill(char *, char *);
void			 summary(void);
void			 onsignal(int);
void			 retransmit(int);
int			 pinger(int);
const char		*pr_addr(struct sockaddr *, socklen_t);
void			 pr_pack(u_char *, int, struct msghdr *);
__dead void		 usage(void);

void			 pr_ipopt(int, u_char *);
int			 in_cksum(u_short *, int);
void			 pr_icmph(struct icmp *);
void			 pr_retip(struct ip *);
void			 pr_iph(struct ip *);
#ifndef SMALL
int			 map_tos(char *, int *);
#endif	/* SMALL */

int
main(int argc, char *argv[])
{
	struct addrinfo hints, *res;
	struct itimerval itimer;
	struct sockaddr_in  from, from4, dst;
	socklen_t maxsizelen;
	int64_t preload;
	int ch, i, optval = 1, packlen, maxsize, error, s;
	int df = 0, tos = 0, bufspace = IP_MAXPACKET;
	u_char *datap, *packet, loop = 1;
	u_char ttl = MAXTTL;
	char *e, *target, hbuf[NI_MAXHOST], *source = NULL;
	char rspace[3 + 4 * NROUTES + 1];	/* record route space */
	const char *errstr;
	double intval;
	uid_t uid;
	u_int rtableid = 0;

	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0)
		err(1, "socket");

	/* revoke privs */
	uid = getuid();
	if (setresuid(uid, uid, uid) == -1)
		err(1, "setresuid");

	preload = 0;
	datap = &outpack[ECHOLEN + ECHOTMLEN];
	while ((ch = getopt(argc, argv,
	    "DEI:LRS:c:defHi:l:np:qs:T:t:V:vw:")) != -1) {
		switch(ch) {
		case 'c':
			npackets = strtonum(optarg, 0, INT64_MAX, &errstr);
			if (errstr)
				errx(1,
				    "number of packets to transmit is %s: %s",
				    errstr, optarg);
			break;
		case 'D':
			options |= F_HDRINCL;
			df = 1;
			break;
		case 'd':
			options |= F_SO_DEBUG;
			break;
		case 'E':
			options |= F_AUD_MISS;
			break;
		case 'e':
			options |= F_AUD_RECV;
			break;
		case 'f':
			if (getuid())
				errx(1, "%s", strerror(EPERM));
			options |= F_FLOOD;
			setvbuf(stdout, NULL, _IONBF, 0);
			break;
		case 'H':
			options |= F_HOSTNAME;
			break;
		case 'I':
		case 'S':	/* deprecated */
			source = optarg;
			break;
		case 'i':		/* wait between sending packets */
			intval = strtod(optarg, &e);
			if (*optarg == '\0' || *e != '\0')
				errx(1, "illegal timing interval %s", optarg);
			if (intval < 1 && getuid()) {
				errx(1, "%s: only root may use interval < 1s",
				    strerror(EPERM));
			}
			interval.tv_sec = (time_t)intval;
			interval.tv_usec =
			    (long)((intval - interval.tv_sec) * 1000000);
			if (interval.tv_sec < 0)
				errx(1, "illegal timing interval %s", optarg);
			/* less than 1/Hz does not make sense */
			if (interval.tv_sec == 0 && interval.tv_usec < 10000) {
				warnx("too small interval, raised to 0.01");
				interval.tv_usec = 10000;
			}
			options |= F_INTERVAL;
			break;
		case 'L':
			moptions |= MULTICAST_NOLOOP;
			loop = 0;
			break;
		case 'l':
			if (getuid())
				errx(1, "%s", strerror(EPERM));
			preload = strtonum(optarg, 1, INT64_MAX, &errstr);
			if (errstr)
				errx(1, "preload value is %s: %s", errstr,
				    optarg);
			break;
		case 'n':
			options &= ~F_HOSTNAME;
			break;
		case 'p':		/* fill buffer with user pattern */
			options |= F_PINGFILLED;
			fill((char *)datap, optarg);
				break;
		case 'q':
			options |= F_QUIET;
			break;
		case 'R':
			options |= F_RROUTE;
			break;
		case 's':		/* size of packet to send */
			datalen = strtonum(optarg, 0, MAXPAYLOAD, &errstr);
			if (errstr)
				errx(1, "packet size is %s: %s", errstr,
				    optarg);
			break;
#ifndef SMALL
		case 'T':
			options |= F_HDRINCL;
			errno = 0;
			errstr = NULL;
			if (map_tos(optarg, &tos))
				break;
			if (strlen(optarg) > 1 && optarg[0] == '0' &&
			    optarg[1] == 'x')
				tos = (int)strtol(optarg, NULL, 16);
			else
				tos = strtonum(optarg, 0, 255, &errstr);
			if (tos < 0 || tos > 255 || errstr || errno)
				errx(1, "illegal tos value %s", optarg);
			break;
#endif	/* SMALL */
		case 't':
			options |= F_TTL;
			ttl = strtonum(optarg, 0, MAXTTL, &errstr);
			if (errstr)
				errx(1, "ttl value is %s: %s", errstr, optarg);
			break;
		case 'V':
			rtableid = strtonum(optarg, 0, RT_TABLEID_MAX, &errstr);
			if (errstr)
				errx(1, "rtable value is %s: %s", errstr,
				    optarg);
			if (setsockopt(s, SOL_SOCKET, SO_RTABLE, &rtableid,
			    sizeof(rtableid)) == -1)
				err(1, "setsockopt SO_RTABLE");
			break;
		case 'v':
			options |= F_VERBOSE;
			break;
		case 'w':
			maxwait = strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr)
				errx(1, "maxwait value is %s: %s",
				    errstr, optarg);
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	memset(&dst, 0, sizeof(dst));

	if (inet_aton(*argv, &dst.sin_addr) != 0) {
		hostname = *argv;
		if ((target = strdup(inet_ntoa(dst.sin_addr))) == NULL)
			errx(1, "malloc");
	} else
		target = *argv;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_INET;
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = 0;
	hints.ai_flags = AI_CANONNAME;
	if ((error = getaddrinfo(target, NULL, &hints, &res)))
		errx(1, "%s", gai_strerror(error));

	switch (res->ai_family) {
	case AF_INET:
		if (res->ai_addrlen != sizeof(dst))
		    errx(1, "size of sockaddr mismatch");
		break;
	case AF_INET6:
	default:
		errx(1, "unsupported AF: %d", res->ai_family);
		break;
	}

	memcpy(&dst, res->ai_addr, res->ai_addrlen);

	if (!hostname) {
		hostname = res->ai_canonname ? strdup(res->ai_canonname) :
		    target;
		if (!hostname)
			errx(1, "malloc");
	}

	if (res->ai_next) {
		if (getnameinfo(res->ai_addr, res->ai_addrlen, hbuf,
		    sizeof(hbuf), NULL, 0, NI_NUMERICHOST) != 0)
			strlcpy(hbuf, "?", sizeof(hbuf));
		warnx("Warning: %s has multiple "
		    "addresses; using %s", hostname, hbuf);
	}
	freeaddrinfo(res);

	if (source) {
		memset(&from4, 0, sizeof(from4));
		from4.sin_family = AF_INET;
		if (inet_aton(source, &from4.sin_addr) == 0) {
			memset(&hints, 0, sizeof(hints));
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
			if ((error = getaddrinfo(source, NULL, &hints, &res)))
				errx(1, "%s: %s", source, gai_strerror(error));
			if (res->ai_addrlen != sizeof(from4))
				errx(1, "size of sockaddr mismatch");
			memcpy(&from4, res->ai_addr, res->ai_addrlen);
			freeaddrinfo(res);
		}

		if (IN_MULTICAST(ntohl(dst.sin_addr.s_addr))) {
			if (setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF,
			    &from4.sin_addr, sizeof(from4.sin_addr)) < 0)
				err(1, "setsockopt IP_MULTICAST_IF");
		} else {
			if (bind(s, (struct sockaddr *)&from4, sizeof(from4))
			    < 0)
				err(1, "bind");
		}
	}

	if (options & F_SO_DEBUG)
		(void)setsockopt(s, SOL_SOCKET, SO_DEBUG, &optval,
		    sizeof(optval));

	if ((options & F_FLOOD) && (options & F_INTERVAL))
		errx(1, "-f and -i options are incompatible");

	if ((options & F_FLOOD) && (options & (F_AUD_RECV | F_AUD_MISS)))
		warnx("No audible output for flood pings");

	if (datalen >= sizeof(struct payload))	/* can we time transfer */
		timing = 1;
	packlen = datalen + MAXIPLEN + MAXICMPLEN;
	if (!(packet = malloc(packlen)))
		err(1, "malloc");

	if (!(options & F_PINGFILLED))
		for (i = ECHOTMLEN; i < datalen; ++i)
			*datap++ = i;

	ident = getpid() & 0xFFFF;

	if (options & F_TTL) {
		if (IN_MULTICAST(ntohl(dst.sin_addr.s_addr)))
			moptions |= MULTICAST_TTL;
		else
			options |= F_HDRINCL;
	}

	if (options & F_RROUTE && options & F_HDRINCL)
		errx(1, "-R option and -D or -T, or -t to unicast destinations"
		    " are incompatible");

	if (options & F_HDRINCL) {
		struct ip *ip = (struct ip *)outpackhdr;

		setsockopt(s, IPPROTO_IP, IP_HDRINCL, &optval, sizeof(optval));
		ip->ip_v = IPVERSION;
		ip->ip_hl = sizeof(struct ip) >> 2;
		ip->ip_tos = tos;
		ip->ip_id = 0;
		ip->ip_off = htons(df ? IP_DF : 0);
		ip->ip_ttl = ttl;
		ip->ip_p = IPPROTO_ICMP;
		if (source)
			ip->ip_src = from4.sin_addr;
		else
			ip->ip_src.s_addr = INADDR_ANY;
		ip->ip_dst = dst.sin_addr;
	}

	/* record route option */
	if (options & F_RROUTE) {
		if (IN_MULTICAST(ntohl(dst.sin_addr.s_addr)))
			errx(1, "record route not valid to multicast destinations");
		memset(rspace, 0, sizeof(rspace));
		rspace[IPOPT_OPTVAL] = IPOPT_RR;
		rspace[IPOPT_OLEN] = sizeof(rspace)-1;
		rspace[IPOPT_OFFSET] = IPOPT_MINOFF;
		if (setsockopt(s, IPPROTO_IP, IP_OPTIONS, rspace,
		    sizeof(rspace)) < 0) {
			perror("ping: record route");
			exit(1);
		}
	}

	if ((moptions & MULTICAST_NOLOOP) &&
	    setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP, &loop,
	    sizeof(loop)) < 0)
		err(1, "setsockopt IP_MULTICAST_LOOP");
	if ((moptions & MULTICAST_TTL) &&
	    setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, &ttl,
	    sizeof(ttl)) < 0)
		err(1, "setsockopt IP_MULTICAST_TTL");

	/*
	 * When trying to send large packets, you must increase the
	 * size of both the send and receive buffers...
	 */
	maxsizelen = sizeof maxsize;
	if (getsockopt(s, SOL_SOCKET, SO_SNDBUF, &maxsize, &maxsizelen) < 0)
		err(1, "getsockopt");
	if (maxsize < packlen &&
	    setsockopt(s, SOL_SOCKET, SO_SNDBUF, &packlen, sizeof(maxsize)) < 0)
		err(1, "setsockopt");

	/*
	 * When pinging the broadcast address, you can get a lot of answers.
	 * Doing something so evil is useful if you are trying to stress the
	 * ethernet, or just want to fill the arp cache to get some stuff for
	 * /etc/ethers.
	 */
	while (setsockopt(s, SOL_SOCKET, SO_RCVBUF,
	    (void*)&bufspace, sizeof(bufspace)) < 0) {
		if ((bufspace -= 1024) <= 0)
			err(1, "Cannot set the receive buffer size");
	}
	if (bufspace < IP_MAXPACKET)
		warnx("Could only allocate a receive buffer of %d bytes (default %d)",
		    bufspace, IP_MAXPACKET);

	if (!source && options & F_VERBOSE) {
		/*
		 * get the source address. XXX since we revoked the root
		 * privilege, we cannot use a raw socket for this.
		 */
		int dummy;
		socklen_t len = sizeof(from4);

		if ((dummy = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
			err(1, "UDP socket");

		from4.sin_family = AF_INET;
		from4.sin_addr = dst.sin_addr;
		from4.sin_port = ntohs(DUMMY_PORT);

		if ((moptions & MULTICAST_NOLOOP) &&
		    setsockopt(dummy, IPPROTO_IP, IP_MULTICAST_LOOP, &loop,
		    sizeof(loop)) < 0)
			err(1, "setsockopt IP_MULTICAST_LOOP");
		if ((moptions & MULTICAST_TTL) &&
		    setsockopt(dummy, IPPROTO_IP, IP_MULTICAST_TTL, &ttl,
		    sizeof(ttl)) < 0)
		err(1, "setsockopt IP_MULTICAST_TTL");

		if (rtableid > 0 &&
		    setsockopt(dummy, SOL_SOCKET, SO_RTABLE, &rtableid,
		    sizeof(rtableid)) < 0)
			err(1, "setsockopt(SO_RTABLE)");

		if (connect(dummy, (struct sockaddr *)&from4, len) < 0)
			err(1, "UDP connect");

		if (getsockname(dummy, (struct sockaddr *)&from4, &len) < 0)
			err(1, "getsockname");

		close(dummy);
	}

	if (options & F_HOSTNAME) {
		if (pledge("stdio inet dns", NULL) == -1)
			err(1, "pledge");
	} else {
		if (pledge("stdio inet", NULL) == -1)
			err(1, "pledge");
	}

	arc4random_buf(&tv64_offset, sizeof(tv64_offset));
	arc4random_buf(&mac_key, sizeof(mac_key));

	printf("PING %s (", hostname);
	if (0 && (options & F_VERBOSE))
		printf("%s --> ", pr_addr((struct sockaddr *)&from4,
		    sizeof(from4)));
	printf("%s): %d data bytes\n", pr_addr((struct sockaddr *)&dst,
	    sizeof(dst)), datalen);

	smsghdr.msg_name = &dst;
	smsghdr.msg_namelen = sizeof(dst);
	smsgiov.iov_base = (caddr_t)outpack;
	smsghdr.msg_iov = &smsgiov;
	smsghdr.msg_iovlen = 1;

	while (preload--)		/* Fire off them quickies. */
		pinger(s);

	(void)signal(SIGINT, onsignal);
	(void)signal(SIGINFO, onsignal);

	if ((options & F_FLOOD) == 0) {
		(void)signal(SIGALRM, onsignal);
		itimer.it_interval = interval;
		itimer.it_value = interval;
		(void)setitimer(ITIMER_REAL, &itimer, NULL);
		if (ntransmitted == 0)
			retransmit(s);
	}

	seenalrm = seenint = 0;
	seeninfo = 0;

	for (;;) {
		struct msghdr	m;
		union {
			struct cmsghdr hdr;
			u_char buf[CMSG_SPACE(1024)];
		}		cmsgbuf;
		struct iovec	iov[1];
		struct pollfd	pfd;
		ssize_t		cc;
		int		timeout;

		/* signal handling */
		if (seenint)
			break;
		if (seenalrm) {
			retransmit(s);
			seenalrm = 0;
			if (ntransmitted - nreceived - 1 > nmissedmax) {
				nmissedmax = ntransmitted - nreceived - 1;
				if (!(options & F_FLOOD) &&
				    (options & F_AUD_MISS))
					(void)fputc('\a', stderr);
			}
			continue;
		}
		if (seeninfo) {
			summary();
			seeninfo = 0;
			continue;
		}

		if (options & F_FLOOD) {
			(void)pinger(s);
			timeout = 10;
		} else
			timeout = INFTIM;

		pfd.fd = s;
		pfd.events = POLLIN;

		if (poll(&pfd, 1, timeout) <= 0)
			continue;

		m.msg_name = &from;
		m.msg_namelen = sizeof(from);
		memset(&iov, 0, sizeof(iov));
		iov[0].iov_base = (caddr_t)packet;
		iov[0].iov_len = packlen;
		m.msg_iov = iov;
		m.msg_iovlen = 1;
		m.msg_control = (caddr_t)&cmsgbuf.buf;
		m.msg_controllen = sizeof(cmsgbuf.buf);

		cc = recvmsg(s, &m, 0);
		if (cc < 0) {
			if (errno != EINTR) {
				warn("recvmsg");
				sleep(1);
			}
			continue;
		} else
			pr_pack(packet, cc, &m);

		if (npackets && nreceived >= npackets)
			break;
	}
	summary();
	exit(nreceived == 0);
}

void
onsignal(int sig)
{
	switch (sig) {
	case SIGALRM:
		seenalrm++;
		break;
	case SIGINT:
		seenint++;
		break;
	case SIGINFO:
		seeninfo++;
		break;
	}
}

void
fill(char *bp, char *patp)
{
	int ii, jj, kk;
	int pat[16];
	char *cp;

	for (cp = patp; *cp; cp++)
		if (!isxdigit((unsigned char)*cp))
			errx(1, "patterns must be specified as hex digits");
	ii = sscanf(patp,
	    "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
	    &pat[0], &pat[1], &pat[2], &pat[3], &pat[4], &pat[5], &pat[6],
	    &pat[7], &pat[8], &pat[9], &pat[10], &pat[11], &pat[12],
	    &pat[13], &pat[14], &pat[15]);

	if (ii > 0)
		for (kk = 0;
		    kk <= MAXPAYLOAD - (ECHOLEN + ECHOTMLEN + ii);
		    kk += ii)
			for (jj = 0; jj < ii; ++jj)
				bp[jj + kk] = pat[jj];
	if (!(options & F_QUIET)) {
		(void)printf("PATTERN: 0x");
		for (jj = 0; jj < ii; ++jj)
			(void)printf("%02x", bp[jj] & 0xFF);
		(void)printf("\n");
	}
}

void
summary(void)
{
	printf("\n--- %s ping statistics ---\n", hostname);
	printf("%lld packets transmitted, ", ntransmitted);
	printf("%lld packets received, ", nreceived);

	if (nrepeats)
		printf("%lld duplicates, ", nrepeats);
	if (ntransmitted) {
		if (nreceived > ntransmitted)
			printf("-- somebody's duplicating packets!");
		else
			printf("%.1f%% packet loss",
			    ((((double)ntransmitted - nreceived) * 100) /
			    ntransmitted));
	}
	printf("\n");
	if (timinginfo) {
		/* Only display average to microseconds */
		double num = nreceived + nrepeats;
		double avg = tsum / num;
		double dev = sqrt(fmax(0, tsumsq / num - avg * avg));
		printf("round-trip min/avg/max/std-dev = %.3f/%.3f/%.3f/%.3f ms\n",
		    tmin, avg, tmax, dev);
	}
}

/*
 * pr_addr --
 *	Return address in numeric form or a host name
 */
const char *
pr_addr(struct sockaddr *addr, socklen_t addrlen)
{
	static char buf[NI_MAXHOST];
	int flag = 0;

	if ((options & F_HOSTNAME) == 0)
		flag |= NI_NUMERICHOST;

	if (getnameinfo(addr, addrlen, buf, sizeof(buf), NULL, 0, flag) == 0)
		return (buf);
	else
		return "?";
}

/*
 * retransmit --
 *	This routine transmits another ping.
 */
void
retransmit(int s)
{
	struct itimerval itimer;
	static int last_time = 0;

	if (last_time) {
		seenint = 1;	/* break out of ping event loop */
		return;
	}

	if (pinger(s) == 0)
		return;

	/*
	 * If we're not transmitting any more packets, change the timer
	 * to wait two round-trip times if we've received any packets or
	 * maxwait seconds if we haven't.
	 */
	if (nreceived) {
		itimer.it_value.tv_sec =  2 * tmax / 1000;
		if (itimer.it_value.tv_sec == 0)
			itimer.it_value.tv_sec = 1;
	} else
		itimer.it_value.tv_sec = maxwait;
	itimer.it_interval.tv_sec = 0;
	itimer.it_interval.tv_usec = 0;
	itimer.it_value.tv_usec = 0;
	(void)setitimer(ITIMER_REAL, &itimer, NULL);

	/* When the alarm goes off we are done. */
	last_time = 1;
}

/*
 * pinger --
 *	Compose and transmit an ICMP ECHO REQUEST packet.  The IP packet
 * will be added on by the kernel.  The ID field is our UNIX process ID,
 * and the sequence number is an ascending integer.  The first 8 bytes
 * of the data portion are used to hold a UNIX "timeval" struct in VAX
 * byte-order, to compute the round-trip time.
 */
int
pinger(int s)
{
	struct icmp *icp;
	int cc, i;
	u_int16_t seq;

	if (npackets && ntransmitted >= npackets)
		return(-1);	/* no more transmission */

	seq = htons(ntransmitted++);

	icp = (struct icmp *)outpack;
	icp->icmp_type = ICMP_ECHO;
	icp->icmp_code = 0;
	icp->icmp_cksum = 0;
	icp->icmp_seq = seq;
	icp->icmp_id = ident;			/* ID */

	CLR(ntohs(seq) % mx_dup_ck);

	if (timing) {
		SIPHASH_CTX ctx;
		struct timespec ts;
		struct payload payload;
		struct tv64 *tv64 = &payload.tv64;

		if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1)
			err(1, "clock_gettime(CLOCK_MONOTONIC)");
		tv64->tv64_sec = htobe64((u_int64_t)ts.tv_sec +
		    tv64_offset.tv64_sec);
		tv64->tv64_nsec = htobe64((u_int64_t)ts.tv_nsec +
		    tv64_offset.tv64_nsec);

		SipHash24_Init(&ctx, &mac_key);
		SipHash24_Update(&ctx, tv64, sizeof(*tv64));
		SipHash24_Update(&ctx, &ident, sizeof(ident));
		SipHash24_Update(&ctx, &seq, sizeof(seq));
		SipHash24_Final(&payload.mac, &ctx);

		memcpy(&outpack[ECHOLEN], &payload, sizeof(payload));
	}

	cc = datalen + ECHOLEN;			/* skips ICMP portion */

	/* compute ICMP checksum here */
	icp->icmp_cksum = in_cksum((u_short *)icp, cc);

	if (options & F_HDRINCL) {
		struct ip *ip = (struct ip *)outpackhdr;

		smsgiov.iov_base = (caddr_t)outpackhdr;
		cc += sizeof(struct ip);
		ip->ip_len = htons(cc);
		ip->ip_sum = in_cksum((u_short *)outpackhdr, cc);
	} else
		smsgiov.iov_base = (caddr_t)outpack;

	smsgiov.iov_len = cc;

	i = sendmsg(s, &smsghdr, 0);

	if (i < 0 || i != cc)  {
		if (i < 0)
			warn("sendmsg");
		printf("ping: wrote %s %d chars, ret=%d\n", hostname, cc, i);
	}
	if (!(options & F_QUIET) && options & F_FLOOD)
		(void)write(STDOUT_FILENO, &DOT, 1);

	return (0);
}

/*
 * pr_pack --
 *	Print out the packet, if it came from us.  This logic is necessary
 * because ALL readers of the ICMP socket get a copy of ALL ICMP packets
 * which arrive ('tis only fair).  This permits multiple copies of this
 * program to be run without having intermingled output (or statistics!).
 */
void
pr_pack(u_char *buf, int cc, struct msghdr *mhdr)
{
	struct ip *ip;
	struct icmp *icp;
	struct timespec ts, tp;
	struct payload payload;
	struct sockaddr *from;
	socklen_t fromlen;
	double triptime = 0;
	int i, dupflag;
	int hlen;
	u_int16_t seq;
	u_char *cp, *dp;
	char* pkttime;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1)
		err(1, "clock_gettime(CLOCK_MONOTONIC)");

	if (!mhdr || !mhdr->msg_name ||
	    mhdr->msg_namelen != sizeof(struct sockaddr_in) ||
	    ((struct sockaddr *)mhdr->msg_name)->sa_family != AF_INET) {
		if (options & F_VERBOSE)
			warnx("invalid peername");
		return;
	}
	from = (struct sockaddr *)mhdr->msg_name;
	fromlen = mhdr->msg_namelen;

	/* Check the IP header */
	ip = (struct ip *)buf;
	hlen = ip->ip_hl << 2;
	if (cc < hlen + ICMP_MINLEN) {
		if (options & F_VERBOSE)
			warnx("packet too short (%d bytes) from %s", cc,
			    pr_addr(from, fromlen));
		return;
	}

	/* Now the ICMP part */
	cc -= hlen;
	icp = (struct icmp *)(buf + hlen);
	if (icp->icmp_type == ICMP_ECHOREPLY) {
		if (icp->icmp_id != ident)
			return;			/* 'Twas not our ECHO */
		seq = icp->icmp_seq;
		++nreceived;
		if (cc >= ECHOLEN + ECHOTMLEN) {
			SIPHASH_CTX ctx;
			struct tv64 *tv64;
			u_int8_t mac[SIPHASH_DIGEST_LENGTH];

			pkttime = (char *)icp->icmp_data;
			memcpy(&payload, pkttime, sizeof(payload));
			tv64 = &payload.tv64;

			SipHash24_Init(&ctx, &mac_key);
			SipHash24_Update(&ctx, tv64, sizeof(*tv64));
			SipHash24_Update(&ctx, &ident, sizeof(ident));
			SipHash24_Update(&ctx, &seq, sizeof(seq));
			SipHash24_Final(mac, &ctx);

			if (timingsafe_memcmp(mac, &payload.mac,
			    sizeof(mac)) != 0) {
				(void)printf("signature mismatch!\n");
				return;
			}
			timinginfo=1;

			tp.tv_sec = betoh64(tv64->tv64_sec) -
			    tv64_offset.tv64_sec;
			tp.tv_nsec = betoh64(tv64->tv64_nsec) -
			    tv64_offset.tv64_nsec;

			timespecsub(&ts, &tp, &ts);
			triptime = ((double)ts.tv_sec) * 1000.0 +
			    ((double)ts.tv_nsec) / 1000000.0;
			tsum += triptime;
			tsumsq += triptime * triptime;
			if (triptime < tmin)
				tmin = triptime;
			if (triptime > tmax)
				tmax = triptime;
		}

		if (TST(ntohs(seq) % mx_dup_ck)) {
			++nrepeats;
			--nreceived;
			dupflag = 1;
		} else {
			SET(ntohs(seq) % mx_dup_ck);
			dupflag = 0;
		}

		if (options & F_QUIET)
			return;

		if (options & F_FLOOD)
			(void)write(STDOUT_FILENO, &BSPACE, 1);
		else {
			(void)printf("%d bytes from %s: icmp_seq=%u", cc,
			    pr_addr(from, fromlen), ntohs(seq));
			(void)printf(" ttl=%d", ip->ip_ttl);
			if (cc >= ECHOLEN + ECHOTMLEN)
				(void)printf(" time=%.3f ms", triptime);
			if (dupflag)
				(void)printf(" (DUP!)");
			/* check the data */
			if (cc - ECHOLEN < datalen)
				(void)printf(" (TRUNC!)");
			cp = (u_char *)&icp->icmp_data[sizeof(struct payload)];
			dp = &outpack[ECHOLEN + ECHOTMLEN];
			for (i = ECHOLEN + ECHOTMLEN;
			    i < cc && i < datalen;
			    ++i, ++cp, ++dp) {
				if (*cp != *dp) {
					(void)printf("\nwrong data byte #%d "
					    "should be 0x%x but was 0x%x",
					    i - ECHOLEN, *dp, *cp);
					cp = (u_char *)&icp->icmp_data[0];
					for (i = ECHOLEN; i < cc && i < datalen;
					     ++i, ++cp) {
						if ((i % 32) == 8)
							(void)printf("\n\t");
						(void)printf("%x ", *cp);
					}
					break;
				}
			}
		}
	} else {
		/* We've got something other than an ECHOREPLY */
		if (!(options & F_VERBOSE))
			return;
		(void)printf("%d bytes from %s: ", cc, pr_addr(from, fromlen));
		pr_icmph(icp);
	}

	/* Display any IP options */
	if (hlen > sizeof(struct ip))
		pr_ipopt(hlen, buf);

	if (!(options & F_FLOOD)) {
		(void)putchar('\n');
		(void)fflush(stdout);
		if (options & F_AUD_RECV)
			(void)fputc('\a', stderr);
	}
}

void
pr_ipopt(int hlen, u_char *buf)
{
	static int old_rrlen;
	static char old_rr[MAX_IPOPTLEN];
	struct sockaddr_in s_in;
	in_addr_t l;
	u_int i, j;
	u_char *cp;

	cp = buf + sizeof(struct ip);

	s_in.sin_len = sizeof(s_in);
	s_in.sin_family = AF_INET;

	for (; hlen > sizeof(struct ip); --hlen, ++cp) {
		switch (*cp) {
		case IPOPT_EOL:
			hlen = 0;
			break;
		case IPOPT_LSRR:
			(void)printf("\nLSRR: ");
			hlen -= 2;
			j = *++cp;
			++cp;
			i = 0;
			if (j > IPOPT_MINOFF) {
				for (;;) {
					l = *++cp;
					l = (l<<8) + *++cp;
					l = (l<<8) + *++cp;
					l = (l<<8) + *++cp;
					if (l == 0)
						(void)printf("\t0.0.0.0");
					else {
						s_in.sin_addr.s_addr = ntohl(l);
						(void)printf("\t%s",
						    pr_addr((struct sockaddr*)
						    &s_in, sizeof(s_in)));
					}
					hlen -= 4;
					j -= 4;
					i += 4;
					if (j <= IPOPT_MINOFF)
						break;
					if (i >= MAX_IPOPTLEN) {
						(void)printf("\t(truncated route)");
						break;
					}
					(void)putchar('\n');
				}
			}
			break;
		case IPOPT_RR:
			j = *++cp;		/* get length */
			i = *++cp;		/* and pointer */
			hlen -= 2;
			if (i > j)
				i = j;
			i -= IPOPT_MINOFF;
			if (i <= 0)
				continue;
			if (i == old_rrlen &&
			    cp == buf + sizeof(struct ip) + 2 &&
			    !memcmp(cp, old_rr, i) &&
			    !(options & F_FLOOD)) {
				(void)printf("\t(same route)");
				i = ((i + 3) / 4) * 4;
				hlen -= i;
				cp += i;
				break;
			}
			if (i < MAX_IPOPTLEN) {
				old_rrlen = i;
				memcpy(old_rr, cp, i);
			} else
				old_rrlen = 0;

			(void)printf("\nRR: ");
			j = 0;
			for (;;) {
				l = *++cp;
				l = (l<<8) + *++cp;
				l = (l<<8) + *++cp;
				l = (l<<8) + *++cp;
				if (l == 0)
					(void)printf("\t0.0.0.0");
				else {
					s_in.sin_addr.s_addr = ntohl(l);
					(void)printf("\t%s",
					    pr_addr((struct sockaddr*)&s_in,
					    sizeof(s_in)));
				}
				hlen -= 4;
				i -= 4;
				j += 4;
				if (i <= 0)
					break;
				if (j >= MAX_IPOPTLEN) {
					(void)printf("\t(truncated route)");
					break;
				}
				(void)putchar('\n');
			}
			break;
		case IPOPT_NOP:
			(void)printf("\nNOP");
			break;
		default:
			(void)printf("\nunknown option %x", *cp);
			hlen = hlen - (cp[IPOPT_OLEN] - 1);
			cp = cp + (cp[IPOPT_OLEN] - 1);
			break;
		}
	}
}

/*
 * in_cksum --
 *	Checksum routine for Internet Protocol family headers (C Version)
 */
int
in_cksum(u_short *addr, int len)
{
	int nleft = len;
	u_short *w = addr;
	int sum = 0;
	u_short answer = 0;

	/*
	 * Our algorithm is simple, using a 32 bit accumulator (sum), we add
	 * sequential 16 bit words to it, and at the end, fold back all the
	 * carry bits from the top 16 bits into the lower 16 bits.
	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1) {
		*(u_char *)(&answer) = *(u_char *)w ;
		sum += answer;
	}

	/* add back carry outs from top 16 bits to low 16 bits */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return(answer);
}

/*
 * pr_icmph --
 *	Print a descriptive string about an ICMP header.
 */
void
pr_icmph(struct icmp *icp)
{
	switch(icp->icmp_type) {
	case ICMP_ECHOREPLY:
		(void)printf("Echo Reply\n");
		/* XXX ID + Seq + Data */
		break;
	case ICMP_UNREACH:
		switch(icp->icmp_code) {
		case ICMP_UNREACH_NET:
			(void)printf("Destination Net Unreachable\n");
			break;
		case ICMP_UNREACH_HOST:
			(void)printf("Destination Host Unreachable\n");
			break;
		case ICMP_UNREACH_PROTOCOL:
			(void)printf("Destination Protocol Unreachable\n");
			break;
		case ICMP_UNREACH_PORT:
			(void)printf("Destination Port Unreachable\n");
			break;
		case ICMP_UNREACH_NEEDFRAG:
			if (icp->icmp_nextmtu != 0)
				(void)printf("frag needed and DF set (MTU %d)\n",
				    ntohs(icp->icmp_nextmtu));
			else
				(void)printf("frag needed and DF set\n");
			break;
		case ICMP_UNREACH_SRCFAIL:
			(void)printf("Source Route Failed\n");
			break;
		case ICMP_UNREACH_NET_UNKNOWN:
			(void)printf("Network Unknown\n");
			break;
		case ICMP_UNREACH_HOST_UNKNOWN:
			(void)printf("Host Unknown\n");
			break;
		case ICMP_UNREACH_ISOLATED:
			(void)printf("Source Isolated\n");
			break;
		case ICMP_UNREACH_NET_PROHIB:
			(void)printf("Dest. Net Administratively Prohibited\n");
			break;
		case ICMP_UNREACH_HOST_PROHIB:
			(void)printf("Dest. Host Administratively Prohibited\n");
			break;
		case ICMP_UNREACH_TOSNET:
			(void)printf("Destination Net Unreachable for TOS\n");
			break;
		case ICMP_UNREACH_TOSHOST:
			(void)printf("Destination Host Unreachable for TOS\n");
			break;
		case ICMP_UNREACH_FILTER_PROHIB:
			(void)printf("Route administratively prohibited\n");
			break;
		case ICMP_UNREACH_HOST_PRECEDENCE:
			(void)printf("Host Precedence Violation\n");
			break;
		case ICMP_UNREACH_PRECEDENCE_CUTOFF:
			(void)printf("Precedence Cutoff\n");
			break;
		default:
			(void)printf("Dest Unreachable, Unknown Code: %d\n",
			    icp->icmp_code);
			break;
		}
		/* Print returned IP header information */
		pr_retip((struct ip *)icp->icmp_data);
		break;
	case ICMP_SOURCEQUENCH:
		(void)printf("Source Quench\n");
		pr_retip((struct ip *)icp->icmp_data);
		break;
	case ICMP_REDIRECT:
		switch(icp->icmp_code) {
		case ICMP_REDIRECT_NET:
			(void)printf("Redirect Network");
			break;
		case ICMP_REDIRECT_HOST:
			(void)printf("Redirect Host");
			break;
		case ICMP_REDIRECT_TOSNET:
			(void)printf("Redirect Type of Service and Network");
			break;
		case ICMP_REDIRECT_TOSHOST:
			(void)printf("Redirect Type of Service and Host");
			break;
		default:
			(void)printf("Redirect, Unknown Code: %d", icp->icmp_code);
			break;
		}
		(void)printf("(New addr: %s)\n",
		    inet_ntoa(icp->icmp_gwaddr));
		pr_retip((struct ip *)icp->icmp_data);
		break;
	case ICMP_ECHO:
		(void)printf("Echo Request\n");
		/* XXX ID + Seq + Data */
		break;
	case ICMP_ROUTERADVERT:
		/* RFC1256 */
		(void)printf("Router Discovery Advertisement\n");
		(void)printf("(%d entries, lifetime %d seconds)\n",
		    icp->icmp_num_addrs, ntohs(icp->icmp_lifetime));
		break;
	case ICMP_ROUTERSOLICIT:
		/* RFC1256 */
		(void)printf("Router Discovery Solicitation\n");
		break;
	case ICMP_TIMXCEED:
		switch(icp->icmp_code) {
		case ICMP_TIMXCEED_INTRANS:
			(void)printf("Time to live exceeded\n");
			break;
		case ICMP_TIMXCEED_REASS:
			(void)printf("Frag reassembly time exceeded\n");
			break;
		default:
			(void)printf("Time exceeded, Unknown Code: %d\n",
			    icp->icmp_code);
			break;
		}
		pr_retip((struct ip *)icp->icmp_data);
		break;
	case ICMP_PARAMPROB:
		switch(icp->icmp_code) {
		case ICMP_PARAMPROB_OPTABSENT:
			(void)printf("Parameter problem, required option "
			    "absent: pointer = 0x%02x\n",
			    ntohs(icp->icmp_hun.ih_pptr));
			break;
		default:
			(void)printf("Parameter problem: pointer = 0x%02x\n",
			    ntohs(icp->icmp_hun.ih_pptr));
			break;
		}
		pr_retip((struct ip *)icp->icmp_data);
		break;
	case ICMP_TSTAMP:
		(void)printf("Timestamp\n");
		/* XXX ID + Seq + 3 timestamps */
		break;
	case ICMP_TSTAMPREPLY:
		(void)printf("Timestamp Reply\n");
		/* XXX ID + Seq + 3 timestamps */
		break;
	case ICMP_IREQ:
		(void)printf("Information Request\n");
		/* XXX ID + Seq */
		break;
	case ICMP_IREQREPLY:
		(void)printf("Information Reply\n");
		/* XXX ID + Seq */
		break;
	case ICMP_MASKREQ:
		(void)printf("Address Mask Request\n");
		break;
	case ICMP_MASKREPLY:
		(void)printf("Address Mask Reply (Mask 0x%08x)\n",
		    ntohl(icp->icmp_mask));
		break;
	default:
		(void)printf("Unknown ICMP type: %d\n", icp->icmp_type);
	}
}

/*
 * pr_iph --
 *	Print an IP header with options.
 */
void
pr_iph(struct ip *ip)
{
	int hlen;
	u_char *cp;

	hlen = ip->ip_hl << 2;
	cp = (u_char *)ip + 20;		/* point to options */

	(void)printf("Vr HL TOS  Len   ID Flg  off TTL Pro  cks      Src      Dst Data\n");
	(void)printf(" %1x  %1x  %02x %04x %04x",
	    ip->ip_v, ip->ip_hl, ip->ip_tos, ip->ip_len, ip->ip_id);
	(void)printf("   %1x %04x", ((ip->ip_off) & 0xe000) >> 13,
	    (ip->ip_off) & 0x1fff);
	(void)printf("  %02x  %02x %04x", ip->ip_ttl, ip->ip_p, ip->ip_sum);
	(void)printf(" %s ", inet_ntoa(*(struct in_addr *)&ip->ip_src.s_addr));
	(void)printf(" %s ", inet_ntoa(*(struct in_addr *)&ip->ip_dst.s_addr));
	/* dump and option bytes */
	while (hlen-- > 20) {
		(void)printf("%02x", *cp++);
	}
	(void)putchar('\n');
}

/*
 * pr_retip --
 *	Dump some info on a returned (via ICMP) IP packet.
 */
void
pr_retip(struct ip *ip)
{
	int hlen;
	u_char *cp;

	pr_iph(ip);
	hlen = ip->ip_hl << 2;
	cp = (u_char *)ip + hlen;

	if (ip->ip_p == 6)
		(void)printf("TCP: from port %u, to port %u (decimal)\n",
		    (*cp * 256 + *(cp + 1)), (*(cp + 2) * 256 + *(cp + 3)));
	else if (ip->ip_p == 17)
		(void)printf("UDP: from port %u, to port %u (decimal)\n",
		    (*cp * 256 + *(cp + 1)), (*(cp + 2) * 256 + *(cp + 3)));
}

#ifndef SMALL
int
map_tos(char *key, int *val)
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
		{ NULL, 		-1 },
	};
	
	for (t = toskeywords; t->keyword != NULL; t++) {
		if (strcmp(key, t->keyword) == 0) {
			*val = t->val;
			return (1);
		}
	}
	
	return (0);
}
#endif	/* SMALL */

__dead void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: ping [-DdEefHLnqRv] [-c count] [-I ifaddr] [-i wait]\n"
	    "\t[-l preload] [-p pattern] [-s packetsize]"
#ifndef	SMALL
	    " [-T toskeyword]"
#endif	/* SMALL */
	    "\n\t[-t ttl] [-V rtable] [-w maxwait] host\n");
	exit(1);
}
