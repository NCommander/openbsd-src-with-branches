/*	$OpenBSD: main.c,v 1.50 2005/01/14 15:00:44 mcbride Exp $	*/
/*	$NetBSD: main.c,v 1.9 1996/05/07 02:55:02 thorpej Exp $	*/

/*
 * Copyright (c) 1983, 1988, 1993
 *	Regents of the University of California.  All rights reserved.
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

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983, 1988, 1993\n\
	Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "from: @(#)main.c	8.4 (Berkeley) 3/1/94";
#else
static char *rcsid = "$OpenBSD: main.c,v 1.50 2005/01/14 15:00:44 mcbride Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/protosw.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <ctype.h>
#include <errno.h>
#include <kvm.h>
#include <limits.h>
#include <netdb.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "netstat.h"

struct nlist nl[] = {
#define	N_MBSTAT	0
	{ "_mbstat" },
#define	N_IPSTAT	1
	{ "_ipstat" },
#define	N_TCBTABLE	2
	{ "_tcbtable" },
#define	N_TCPSTAT	3
	{ "_tcpstat" },
#define	N_UDBTABLE	4
	{ "_udbtable" },
#define	N_UDPSTAT	5
	{ "_udpstat" },
#define	N_IFNET		6
	{ "_ifnet" },
#define	N_IMP		7
	{ "_imp_softc" },
#define	N_ICMPSTAT	8
	{ "_icmpstat" },
#define	N_RTSTAT	9
	{ "_rtstat" },
#define	N_UNIXSW	10
	{ "_unixsw" },
#define N_IDP		11
	{ "_nspcb"},
#define N_IDPSTAT	12
	{ "_idpstat"},
#define N_SPPSTAT	13
	{ "_spp_istat"},
#define N_NSERR		14
	{ "_ns_errstat"},
#define	N_CLNPSTAT	15
	{ "_clnp_stat"},
#define	IN_NOTUSED	16
	{ "_tp_inpcb" },
#define	ISO_NOTUSED	16
	{ "_tp_refinfo" },
#define	N_TPSTAT	18
	{ "_tp_stat" },
#define	N_ESISSTAT	19
	{ "_esis_stat"},
#define N_NIMP		20
	{ "_nimp"},
#define N_RTREE		21
	{ "_rt_tables"},
#define N_CLTP		22
	{ "_cltb"},
#define N_CLTPSTAT	23
	{ "_cltpstat"},
#define	N_NFILE		24
	{ "_nfile" },
#define	N_FILE		25
	{ "_file" },
#define N_IGMPSTAT	26
	{ "_igmpstat" },
#define N_MRTPROTO	27
	{ "_ip_mrtproto" },
#define N_MRTSTAT	28
	{ "_mrtstat" },
#define N_MFCHASHTBL	29
	{ "_mfchashtbl" },
#define	N_MFCHASH	30
	{ "_mfchash" },
#define N_VIFTABLE	31
	{ "_viftable" },
#define N_IPX		32
	{ "_ipxcbtable"},
#define N_IPXSTAT	33
	{ "_ipxstat"},
#define N_SPXSTAT	34
	{ "_spx_istat"},
#define N_IPXERR	35
	{ "_ipx_errstat"},
#define N_AHSTAT	36
	{ "_ahstat"},
#define N_ESPSTAT	37
	{ "_espstat"},
#define N_IP4STAT	38
	{ "_ipipstat"},
#define N_DDPSTAT	39
	{ "_ddpstat"},
#define N_DDPCB		40
	{ "_ddpcb"},
#define N_ETHERIPSTAT	41
	{ "_etheripstat"},
#define N_IP6STAT	42
	{ "_ip6stat" },
#define N_ICMP6STAT	43
	{ "_icmp6stat" },
#define N_IPSECSTAT	44
	{ "_ipsecstat" },
#define N_IPSEC6STAT	45
	{ "_ipsec6stat" },
#define N_PIM6STAT	46
	{ "_pim6stat" },
#define N_MRT6PROTO	47
	{ "_ip6_mrtproto" },
#define N_MRT6STAT	48
	{ "_mrt6stat" },
#define N_MF6CTABLE	49
	{ "_mf6ctable" },
#define N_MIF6TABLE	50
	{ "_mif6table" },
#define N_MBPOOL	51
	{ "_mbpool" },
#define N_MCLPOOL	52
	{ "_mclpool" },
#define N_IPCOMPSTAT	53
	{ "_ipcompstat" },
#define N_RIP6STAT	54
	{ "_rip6stat" },
#define N_CARPSTAT	55
	{ "_carpstats" },
#define	N_RAWIPTABLE	56
	{ "_rawcbtable" },
#define	N_RAWIP6TABLE	57
	{ "_rawin6pcbtable" },
#define N_PFSYNCSTAT	58
	{ "_pfsyncstats" },
#define N_PIMSTAT	59
	{ "_pimstat" },
	{ ""},
};

struct protox {
	u_char	pr_index;			/* index into nlist of cb head */
	u_char	pr_sindex;			/* index into nlist of stat block */
	u_char	pr_wanted;			/* 1 if wanted, 0 otherwise */
	void	(*pr_cblocks)(u_long, char *);	/* control blocks printing routine */
	void	(*pr_stats)(u_long, char *);	/* statistics printing routine */
	char	*pr_name;			/* well-known name */
} protox[] = {
	{ N_TCBTABLE,	N_TCPSTAT,	1,	protopr,
	  tcp_stats,	"tcp" },
	{ N_UDBTABLE,	N_UDPSTAT,	1,	protopr,
	  udp_stats,	"udp" },
	{ N_RAWIPTABLE,	N_IPSTAT,	1,	protopr,
	  ip_stats,	"ip" },
	{ -1,		N_ICMPSTAT,	1,	0,
	  icmp_stats,	"icmp" },
	{ -1,		N_IGMPSTAT,	1,	0,
	  igmp_stats,	"igmp" },
	{ -1,		N_AHSTAT,	1,	0,
	  ah_stats,	"ah" },
	{ -1,		N_ESPSTAT,	1,	0,
	  esp_stats,	"esp" },
	{ -1,		N_IP4STAT,	1,	0,
	  ipip_stats,	"ipencap" },
	{ -1,		N_ETHERIPSTAT,	1,	0,
	  etherip_stats,"etherip" },
	{ -1,		N_IPCOMPSTAT,	1,	0,
	  ipcomp_stats,	"ipcomp" },
	{ -1,		N_CARPSTAT,	1,	0,
	  carp_stats,	"carp" },
	{ -1,		N_PFSYNCSTAT,	1,	0,
	  pfsync_stats,	"pfsync" },
	{ -1,		N_PIMSTAT,	1,	0,
	  pim_stats,	"pim" },
	{ -1,		-1,		0,	0,
	  0,		0 }
};

#ifdef INET6
struct protox ip6protox[] = {
	{ N_TCBTABLE,	N_TCPSTAT,	1,	ip6protopr,
	  0,		"tcp" },
	{ N_UDBTABLE,	N_UDPSTAT,	1,	ip6protopr,
	  0,		"udp" },
	{ N_RAWIP6TABLE,N_IP6STAT,	1,	ip6protopr,
	  ip6_stats,	"ip6" },
	{ -1,		N_ICMP6STAT,	1,	0,
	  icmp6_stats,	"icmp6" },
	{ -1,		N_PIM6STAT,	1,	0,
	  pim6_stats,	"pim6" },
	{ -1,		N_RIP6STAT,	1,	0,
	  rip6_stats,	"rip6" },
	{ -1,		-1,		0,	0,
	  0,		0 }
};
#endif

struct protox ipxprotox[] = {
	{ N_IPX,	N_IPXSTAT,	1,	ipxprotopr,
	  ipx_stats,	"ipx" },
	{ N_IPX,	N_SPXSTAT,	1,	ipxprotopr,
	  spx_stats,	"spx" },
	{ -1,		-1,		0,	0,
	  0,		0 }
};

struct protox nsprotox[] = {
	{ N_IDP,	N_IDPSTAT,	1,	nsprotopr,
	  idp_stats,	"idp" },
	{ N_IDP,	N_SPPSTAT,	1,	nsprotopr,
	  spp_stats,	"spp" },
	{ -1,		N_NSERR,	1,	0,
	  nserr_stats,	"ns_err" },
	{ -1,		-1,		0,	0,
	  0,		0 }
};

struct protox atalkprotox[] = {
	{ N_DDPCB,	N_DDPSTAT,	1,	atalkprotopr,
	  ddp_stats,	"ddp" },
	{ -1,		-1,		0,	0,
	  0,		0 }
};

#ifndef INET6
struct protox *protoprotox[] = {
	protox, ipxprotox, nsprotox, atalkprotox, NULL
};
#else
struct protox *protoprotox[] = {
	protox, ip6protox, ipxprotox, nsprotox, atalkprotox, NULL
};
#endif

static void printproto(struct protox *, char *);
static void usage(void);
static struct protox *name2protox(char *);
static struct protox *knownname(char *);

kvm_t *kvmd;

int
main(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;
	struct protoent *p;
	struct protox *tp = NULL; /* for printing cblocks & stats */
	int ch;
	char *nlistf = NULL, *memf = NULL;
	char buf[_POSIX2_LINE_MAX];

	af = AF_UNSPEC;

	while ((ch = getopt(argc, argv, "Aabdf:gI:ilM:mN:np:qrSstuvw:")) != -1)
		switch (ch) {
		case 'A':
			Aflag = 1;
			break;
		case 'a':
			aflag = 1;
			break;
		case 'b':
			bflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'f':
			if (strcmp(optarg, "inet") == 0)
				af = AF_INET;
			else if (strcmp(optarg, "inet6") == 0)
				af = AF_INET6;
			else if (strcmp(optarg, "local") == 0)
				af = AF_LOCAL;
			else if (strcmp(optarg, "unix") == 0)
				af = AF_UNIX;
			else if (strcmp(optarg, "ipx") == 0)
				af = AF_IPX;
			else if (strcmp(optarg, "ns") == 0)
				af = AF_NS;
			else if (strcmp(optarg, "encap") == 0)
				af = PF_KEY;
			else if (strcmp(optarg, "atalk") == 0)
				af = AF_APPLETALK;
			else {
				(void)fprintf(stderr,
				    "%s: %s: unknown address family\n",
				    __progname, optarg);
				exit(1);
			}
			break;
		case 'g':
			gflag = 1;
			break;
		case 'I':
			iflag = 1;
			interface = optarg;
			break;
		case 'i':
			iflag = 1;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'M':
			memf = optarg;
			break;
		case 'm':
			mflag = 1;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'p':
			if ((tp = name2protox(optarg)) == NULL) {
				(void)fprintf(stderr,
				    "%s: %s: unknown protocol\n",
				    __progname, optarg);
				exit(1);
			}
			pflag = 1;
			break;
		case 'q':
			qflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 'S':
			Sflag = 1;
			break;
		case 's':
			++sflag;
			break;
		case 't':
			tflag = 1;
			break;
		case 'u':
			af = AF_UNIX;
			break;
		case 'v':
			vflag = 1;
			break;
		case 'w':
			interval = atoi(optarg);
			iflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	/*
	 * Discard setgid privileges if not the running kernel so that bad
	 * guys can't print interesting stuff from kernel memory.
	 */
	if (nlistf != NULL || memf != NULL) {
		setegid(getgid());
		setgid(getgid());
	}

	if ((kvmd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY,
	    buf)) == NULL) {
		fprintf(stderr, "%s: kvm_open: %s\n", __progname, buf);
		exit(1);
	}
	setegid(getgid());
	setgid(getgid());

#define	BACKWARD_COMPATIBILITY
#ifdef	BACKWARD_COMPATIBILITY
	if (*argv) {
		if (isdigit(**argv)) {
			interval = atoi(*argv);
			if (interval <= 0)
				usage();
			++argv;
			iflag = 1;
		}
		if (*argv) {
			nlistf = *argv;
			if (*++argv)
				memf = *argv;
		}
	}
#endif

	if (kvm_nlist(kvmd, nl) < 0 || nl[0].n_type == 0) {
		if (nlistf)
			fprintf(stderr, "%s: %s: no namelist\n", __progname,
			    nlistf);
		else
			fprintf(stderr, "%s: no namelist\n", __progname);
		exit(1);
	}
	if (mflag) {
		mbpr(nl[N_MBSTAT].n_value, nl[N_MBPOOL].n_value,
		    nl[N_MCLPOOL].n_value);
		exit(0);
	}
	if (pflag) {
		printproto(tp, tp->pr_name);
		exit(0);
	}
	/*
	 * Keep file descriptors open to avoid overhead
	 * of open/close on each call to get* routines.
	 */
	sethostent(1);
	setnetent(1);
	if (iflag) {
		intpr(interval, nl[N_IFNET].n_value);
		exit(0);
	}
	if (rflag) {
		if (sflag)
			rt_stats(nl[N_RTSTAT].n_value);
		else
			routepr(nl[N_RTREE].n_value);
		exit(0);
	}
	if (gflag) {
		if (sflag) {
			if (af == AF_INET || af == AF_UNSPEC)
				mrt_stats(nl[N_MRTPROTO].n_value,
				    nl[N_MRTSTAT].n_value);
#ifdef INET6
			if (af == AF_INET6 || af == AF_UNSPEC)
				mrt6_stats(nl[N_MRT6PROTO].n_value,
				    nl[N_MRT6STAT].n_value);
#endif
		}
		else {
			if (af == AF_INET || af == AF_UNSPEC)
				mroutepr(nl[N_MRTPROTO].n_value,
				    nl[N_MFCHASHTBL].n_value,
				    nl[N_MFCHASH].n_value,
				    nl[N_VIFTABLE].n_value);
#ifdef INET6
			if (af == AF_INET6 || af == AF_UNSPEC)
				mroute6pr(nl[N_MRT6PROTO].n_value,
				    nl[N_MF6CTABLE].n_value,
				    nl[N_MIF6TABLE].n_value);
#endif
		}
		exit(0);
	}
	if (af == AF_INET || af == AF_UNSPEC) {
		setprotoent(1);
		setservent(1);
		/* ugh, this is O(MN) ... why do we do this? */
		while ((p = getprotoent())) {
			for (tp = protox; tp->pr_name; tp++)
				if (strcmp(tp->pr_name, p->p_name) == 0)
					break;
			if (tp->pr_name == 0 || tp->pr_wanted == 0)
				continue;
			printproto(tp, p->p_name);
		}
		endprotoent();
	}
#ifdef INET6
	if (af == AF_INET6 || af == AF_UNSPEC)
		for (tp = ip6protox; tp->pr_name; tp++)
			printproto(tp, tp->pr_name);
#endif
	if (af == AF_IPX || af == AF_UNSPEC)
		for (tp = ipxprotox; tp->pr_name; tp++)
			printproto(tp, tp->pr_name);
	if (af == AF_NS || af == AF_UNSPEC)
		for (tp = nsprotox; tp->pr_name; tp++)
			printproto(tp, tp->pr_name);
	if ((af == AF_UNIX || af == AF_UNSPEC) && !sflag)
		unixpr(nl[N_UNIXSW].n_value);
	if (af == AF_APPLETALK || af == AF_UNSPEC)
		for (tp = atalkprotox; tp->pr_name; tp++)
			printproto(tp, tp->pr_name);
	exit(0);
}

/*
 * Print out protocol statistics or control blocks (per sflag).
 * If the interface was not specifically requested, and the symbol
 * is not in the namelist, ignore this one.
 */
static void
printproto(struct protox *tp, char *name)
{
	void (*pr)(u_long, char *);
	u_char i;

	if (sflag) {
		pr = tp->pr_stats;
		i = tp->pr_sindex;
	} else {
		pr = tp->pr_cblocks;
		i = tp->pr_index;
	}
	if (pr != NULL && i < sizeof(nl) / sizeof(nl[0]) &&
	    (nl[i].n_value || af != AF_UNSPEC))
		(*pr)(nl[i].n_value, name);
}

/*
 * Read kernel memory, return 0 on success.
 */
int
kread(u_long addr, char *buf, int size)
{

	if (kvm_read(kvmd, addr, buf, size) != size) {
		(void)fprintf(stderr, "%s: %s\n", __progname,
		    kvm_geterr(kvmd));
		return (-1);
	}
	return (0);
}

char *
plural(int n)
{
	return (n != 1 ? "s" : "");
}

char *
plurales(int n)
{
	return (n != 1 ? "es" : "");
}

/*
 * Find the protox for the given "well-known" name.
 */
static struct protox *
knownname(char *name)
{
	struct protox **tpp, *tp;

	for (tpp = protoprotox; *tpp; tpp++)
		for (tp = *tpp; tp->pr_name; tp++)
			if (strcmp(tp->pr_name, name) == 0)
				return (tp);
	return (NULL);
}

/*
 * Find the protox corresponding to name.
 */
static struct protox *
name2protox(char *name)
{
	struct protox *tp;
	char **alias;			/* alias from p->aliases */
	struct protoent *p;

	/*
	 * Try to find the name in the list of "well-known" names. If that
	 * fails, check if name is an alias for an Internet protocol.
	 */
	if ((tp = knownname(name)))
		return (tp);

	setprotoent(1);			/* make protocol lookup cheaper */
	while ((p = getprotoent())) {
		/* assert: name not same as p->name */
		for (alias = p->p_aliases; *alias; alias++)
			if (strcmp(name, *alias) == 0) {
				endprotoent();
				return (knownname(p->p_name));
			}
	}
	endprotoent();
	return (NULL);
}

static void
usage(void)
{
	(void)fprintf(stderr,
"usage: %s [-Aan] [-f address_family] [-M core] [-N system]\n", __progname);
	(void)fprintf(stderr,
"       %s [-bdgilmnqrSstu] [-f address_family] [-M core] [-N system]\n", __progname);
	(void)fprintf(stderr,
"       %s [-bdn] [-I interface] [-M core] [-N system] [-w wait]\n", __progname);
	(void)fprintf(stderr,
"       %s [-s] [-M core] [-N system] [-p protocol]\n", __progname);
	(void)fprintf(stderr,
"       %s [-a] [-f address_family] [-i | -I interface]\n", __progname);
	exit(1);
}
