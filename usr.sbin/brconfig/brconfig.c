/*	$OpenBSD: brconfig.c,v 1.3 1999/03/01 04:44:44 jason Exp $	*/

/*
 * Copyright (c) 1999 Jason L. Wright (jason@thought.net)
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <net/if_bridge.h>
#include <sys/errno.h>
#include <string.h>
#include <err.h>
#include <sysexits.h>
#include <stdlib.h>
#include <limits.h>

void usage(void);
int main(int, char **);
int bridge_setflag(int, char *, short);
int bridge_clrflag(int, char *, short);
int bridge_list(int, char *, char *);
int bridge_addrs(int, char *, char *);
int bridge_maxaddr(int, char *, char *);
int bridge_timeout(int, char *, char *);
int bridge_add(int, char *, char *);
int bridge_delete(int, char *, char *);
int bridge_status(int, char *);
int is_bridge(int, char *);
int bridge_show_all(int);
void printb(char *, unsigned short, char *);

/* if_flags bits: borrowed from ifconfig.c */
#define	IFFBITS \
"\020\1UP\2BROADCAST\3DEBUG\4LOOPBACK\5POINTOPOINT\6NOTRAILERS\7RUNNING\10NOARP\
\11PROMISC\12ALLMULTI\13OACTIVE\14SIMPLEX\15LINK0\16LINK1\17LINK2\20MULTICAST"

#define	IFBABITS	"\020\1BLACKHOLE\2STATIC\3DYNAMIC"

void
usage() {
	fprintf(stderr, "brconfig -a\n");
	fprintf(stderr,
	    "brconfig interface [up] [down] [add interface] [delete interface] ...\n");
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int error = 0, sock;
	char *brdg;

	if (argc < 2) {
		usage();
		return (EX_USAGE);
	}

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0)
		err(1, "socket");

	argc--; argv++;
	brdg = argv[0];

	if (strcmp(brdg, "-a") == 0)
		return bridge_show_all(sock);

	if (!is_bridge(sock, brdg)) {
		warnx("%s is not a bridge", brdg);
		return (EX_USAGE);
	}

	if (argc == 1) {
		error = bridge_status(sock, brdg);
		return (error);
	}

	for (argc--, argv++; argc != 0; argc--, argv++) {
		if (strcmp("add", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("add requires an argument");
				return (EX_USAGE);
			}
			error = bridge_add(sock, brdg, argv[0]);
			if (error)
				return (error);
		}
		else if (strcmp("delete", argv[0]) == 0 ||
		    strcmp("del", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("delete requires an argument");
				return (EX_USAGE);
			}
			error = bridge_delete(sock, brdg, argv[0]);
			if (error)
				return (error);
		}
		else if (strcmp("up", argv[0]) == 0) {
			error = bridge_setflag(sock, brdg, IFF_UP);
			if (error)
				return (error);
		}
		else if (strcmp("down", argv[0]) == 0) {
			error = bridge_clrflag(sock, brdg, IFF_UP);
			if (error)
				return (error);
		}
		else if (strcmp("link0", argv[0]) == 0) {
			error = bridge_setflag(sock, brdg, IFF_LINK0);
			if (error)
				return (error);
		}
		else if (strcmp("-link0", argv[0]) == 0) {
			error = bridge_clrflag(sock, brdg, IFF_LINK0);
			if (error)
				return (error);
		}
		else if (strcmp("link1", argv[0]) == 0) {
			error = bridge_setflag(sock, brdg, IFF_LINK1);
			if (error)
				return (error);
		}
		else if (strcmp("-link1", argv[0]) == 0) {
			error = bridge_clrflag(sock, brdg, IFF_LINK1);
			if (error)
				return (error);
		}
		else if (strcmp("addr", argv[0]) == 0) {
			error = bridge_addrs(sock, brdg, "");
			if (error)
				return (error);
		}
		else if (strcmp("maxaddr", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("maxaddr requires an argument");
				return (EX_USAGE);
			}
			error = bridge_maxaddr(sock, brdg, argv[0]);
			if (error)
				return (error);
		}
		else if (strcmp("timeout", argv[0]) == 0) {
			argc--; argv++;
			if (argc == 0) {
				warnx("timeout requires an argument");
				return (EX_USAGE);
			}
			error = bridge_timeout(sock, brdg, argv[0]);
			if (error)
				return (error);
		}
		else {
			warnx("unrecognized option: %s", argv[0]);
			return (EX_USAGE);
		}
	}

	return (0);
}

int
bridge_show_all(s)
	int s;
{
	char *inbuf = NULL;
	struct ifconf ifc;
	struct ifreq *ifrp, ifreq;
	int len = 8192, i;

	while (1) {
		ifc.ifc_len = len;
		ifc.ifc_buf = inbuf = realloc(inbuf, len);
		if (inbuf == NULL)
			err(1, "malloc");
		if (ioctl(s, SIOCGIFCONF, &ifc) < 0)
			err(1, "ioctl(SIOCGIFCONF)");
		if (ifc.ifc_len + sizeof(struct ifreq) < len)
			break;
		len *= 2;
	}
	ifrp = ifc.ifc_req;
	ifreq.ifr_name[0] = '\0';
	for (i = 0; i < ifc.ifc_len; ) {
		ifrp = (struct ifreq *)((caddr_t)ifc.ifc_req + i);
		i += sizeof(ifrp->ifr_name) +
		    (ifrp->ifr_addr.sa_len > sizeof(struct sockaddr) ?
		    ifrp->ifr_addr.sa_len : sizeof(struct sockaddr));
		if (ifrp->ifr_addr.sa_family != AF_LINK)
			continue;
		if (!is_bridge(s, ifrp->ifr_name))
			continue;
		bridge_status(s, ifrp->ifr_name);
	}
	return (0);
}

int
bridge_setflag(s, brdg, f)
	int s;
	char *brdg;
	short f;
{
	struct ifreq ifr;

	strncpy(ifr.ifr_name, brdg, sizeof ifr.ifr_name);

	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		warn("ioctl(SIOCGIFFLAGS)");
		if (errno == EPERM)
			return (EX_NOPERM);
		return (EX_IOERR);
	}

	ifr.ifr_flags |= f;

	if (ioctl(s, SIOCSIFFLAGS, (caddr_t)&ifr) < 0) {
		warn("ioctl(SIOCSIFFLAGS)");
		if (errno == EPERM)
			return (EX_NOPERM);
		return (EX_IOERR);
	}

	return (0);
}

int
bridge_clrflag(s, brdg, f)
	int s;
	char *brdg;
	short f;
{
	struct ifreq ifr;

	strncpy(ifr.ifr_name, brdg, sizeof ifr.ifr_name);

	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		warn("ioctl(SIOCGIFFLAGS)");
		if (errno == EPERM)
			return (EX_NOPERM);
		return (EX_IOERR);
	}

	ifr.ifr_flags &= ~(f);

	if (ioctl(s, SIOCSIFFLAGS, (caddr_t)&ifr) < 0) {
		warn("ioctl(SIOCSIFFLAGS)");
		if (errno == EPERM)
			return (EX_NOPERM);
		return (EX_IOERR);
	}

	return (0);
}

int
bridge_list(s, brdg, delim)
	int s;
	char *brdg, *delim;
{
	struct ifbreq *reqp;
	struct ifbifconf bifc;
	int i, len = 8192;
	char buf[sizeof(reqp->ifbr_ifsname) + 1], *inbuf = NULL;

	while (1) {
		strncpy(bifc.ifbic_name, brdg, sizeof(bifc.ifbic_name));
		bifc.ifbic_len = len;
		bifc.ifbic_buf = inbuf = realloc(inbuf, len);
		if (inbuf == NULL)
			err(1, "malloc");
		if (ioctl(s, SIOCBRDGIFS, &bifc) < 0)
			err(1, "ioctl(SIOCBRDGIFS)");
		if (bifc.ifbic_len + sizeof(*reqp) < len)
			break;
		len *= 2;
	}
	for (i = 0; i < bifc.ifbic_len / sizeof(*reqp); i++) {
		reqp = bifc.ifbic_req + i;
		bzero(buf, sizeof(buf));
		strncpy(buf, reqp->ifbr_ifsname, sizeof(reqp->ifbr_ifsname));
		printf("%s%s\n", delim, buf);
	}
	free(bifc.ifbic_buf);
	return (0);             /* NOTREACHED */
}

int
bridge_add(s, brdg, ifn)
	int s;
	char *brdg, *ifn;
{
	struct ifbreq req;

	strncpy(req.ifbr_name, brdg, sizeof(req.ifbr_name));
	strncpy(req.ifbr_ifsname, ifn, sizeof(req.ifbr_ifsname));
	if (ioctl(s, SIOCBRDGADD, &req) < 0) {
		warn("ioctl(SIOCADDBRDG)");
		if (errno == EPERM)
			return (EX_NOPERM);
		return (EX_IOERR);
	}
	return (0);
}

int
bridge_delete(s, brdg, ifn)
	int s;
	char *brdg, *ifn;
{
	struct ifbreq req;

	strncpy(req.ifbr_name, brdg, sizeof(req.ifbr_name));
	strncpy(req.ifbr_ifsname, ifn, sizeof(req.ifbr_ifsname));
	if (ioctl(s, SIOCBRDGDEL, &req) < 0) {
		warn("ioctl(SIOCDELBRDG)");
		if (errno == EPERM)
			return (EX_NOPERM);
		return (EX_IOERR);
	}
	return (0);
}

int
bridge_timeout(s, brdg, arg)
	int s;
	char *brdg, *arg;
{
	struct ifbcachetoreq ifbct;
	u_int32_t newtime;
	char *endptr;

	newtime = strtoul(arg, &endptr, 0);
	if (arg[0] == '\0' || endptr[0] != '\0') {
		printf("invalid arg for timeout: %s\n", arg);
		return (EX_USAGE);
	}

	strncpy(ifbct.ifbct_name, brdg, sizeof ifbct.ifbct_name);
	ifbct.ifbct_time = newtime;
	if (ioctl(s, SIOCBRDGSTO, (caddr_t)&ifbct) < 0) {
		warn("ioctl(SIOCBRDGGCACHE)");
		return (EX_IOERR);
	}
	return (0);
}

int
bridge_maxaddr(s, brdg, arg)
	int s;
	char *brdg, *arg;
{
	struct ifbcachereq ifbc;
	u_int32_t newsize;
	char *endptr;

	newsize = strtoul(arg, &endptr, 0);
	if (arg[0] == '\0' || endptr[0] != '\0') {
		printf("invalid arg for maxaddr: %s\n", arg);
		return (EX_USAGE);
	}

	strncpy(ifbc.ifbc_name, brdg, sizeof ifbc.ifbc_name);
	ifbc.ifbc_size = newsize;
	if (ioctl(s, SIOCBRDGSCACHE, (caddr_t)&ifbc) < 0) {
		warn("ioctl(SIOCBRDGGCACHE)");
		return (EX_IOERR);
	}
	return (0);
}

int
bridge_addrs(s, brdg, delim)
	int s;
	char *brdg, *delim;
{
	struct ifbaconf ifbac;
	struct ifbareq *ifba;
	char *inbuf = NULL, buf[sizeof(ifba->ifba_name) + 1];
	int i, len = 8192;

	while (1) {
		ifbac.ifbac_len = len;
		ifbac.ifbac_buf = inbuf = realloc(inbuf, len);
		strncpy(ifbac.ifbac_name, brdg, sizeof(ifbac.ifbac_name));
		if (inbuf == NULL)
			err(EX_IOERR, "malloc");
		if (ioctl(s, SIOCBRDGRTS, &ifbac) < 0) {
			if (errno == ENETDOWN)
				return (0);
			err(EX_IOERR, "ioctl(SIOCBRDGRTS)");
		}
		if (ifbac.ifbac_len + sizeof(*ifba) < len)
			break;
		len *= 2;
	}

	for (i = 0; i < ifbac.ifbac_len / sizeof(*ifba); i++) {
		ifba = ifbac.ifbac_req + i;
		bzero(buf, sizeof(buf));
		strncpy(buf, ifba->ifba_name, sizeof(ifba->ifba_name));
		printf("%s%s %s %u ", delim, ether_ntoa(&ifba->ifba_dst),
		    buf, ifba->ifba_age);
#if 0
		printb("flags", ifba->ifba_flags, IFBABITS);
#endif
		printf("\n");
	}

	return (0);
}

/*
 * Check to make sure 'brdg' is really a bridge interface.
 */
int
is_bridge(s, brdg)
	int s;
	char *brdg;
{
	struct ifreq ifr;
	struct ifbaconf ifbac;

	strncpy(ifr.ifr_name, brdg, sizeof ifr.ifr_name);

	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		warn("ioctl(SIOCGIFFLAGS)");
		return (0);
	}

	ifbac.ifbac_len = 0;
	strncpy(ifbac.ifbac_name, brdg, sizeof(ifbac.ifbac_name));
	if (ioctl(s, SIOCBRDGRTS, (caddr_t)&ifbac) < 0) {
		if (errno == ENETDOWN)
			return (1);
		return (0);
	}
	return (1);
}

int
bridge_status(s, brdg)
	int s;
	char *brdg;
{
	struct ifreq ifr;
	struct ifbcachereq ifbc;
	struct ifbcachetoreq ifbct;
	int err;

	strncpy(ifr.ifr_name, brdg, sizeof ifr.ifr_name);

	if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		warn("ioctl(SIOCGIFFLAGS)");
		if (errno == EPERM)
			return (EX_NOPERM);
		return (EX_IOERR);
	}

	printf("%s: ", brdg);
	printb("flags", ifr.ifr_flags, IFFBITS);
	printf("\n");

	printf("\tInterfaces:\n");
	err = bridge_list(s, brdg, "\t\t");
	if (err)
		return (err);

	strncpy(ifbc.ifbc_name, brdg, sizeof ifbc.ifbc_name);
	if (ioctl(s, SIOCBRDGGCACHE, (caddr_t)&ifbc) < 0) {
		warn("ioctl(SIOCBRDGGCACHE)");
		return (EX_IOERR);
	}

	strncpy(ifbct.ifbct_name, brdg, sizeof ifbct.ifbct_name);
	if (ioctl(s, SIOCBRDGGTO, (caddr_t)&ifbct) < 0) {
		warn("ioctl(SIOCBRDGGTO)");
		return (EX_IOERR);
	}

	printf("\tAddresses (max cache: %u, timeout: %u):\n",
	    ifbc.ifbc_size, ifbct.ifbct_time);

	err = bridge_addrs(s, brdg, "\t\t");
	return (err);
}

/*
 * Print a value a la the %b format of the kernel's printf
 * (borrowed from ifconfig.c)
 */
void
printb(s, v, bits)
	char *s;
	char *bits;
	unsigned short v;
{
	register int i, any = 0;
	register char c;

	if (bits && *bits == 8)
		printf("%s=%o", s, v);
	else
		printf("%s=%x", s, v);
	bits++;
	if (bits) {
		putchar('<');
		while ((i = *bits++)) {
			if (v & (1 << (i-1))) {
				if (any)
					putchar(',');
				any = 1;
				for (; (c = *bits) > 32; bits++)
					putchar(c);
			} else
				for (; *bits > 32; bits++)
					;
		}
		putchar('>');
	}
}
