/*
 * (C)opyright 1995 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 */
#include <stdio.h>
#include <ctype.h>
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
#include <netdb.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include "ipf.h"
#include "ipt.h"

#if !defined(lint) && defined(LIBC_SCCS)
static	char	sccsid[] = "@(#)ipft_hx.c	1.1 3/9/96 (C) 1996 Darren Reed";
static	char	rcsid[] = "$Id: ipft_hx.c,v 2.0.1.2 1997/02/04 13:57:56 darrenr Exp $";
#endif

extern	int	opts;
extern	u_short	portnum();
extern	u_long	buildopts();

static	int	hex_open(), hex_close(), hex_readip();
static	char	*readhex();

struct	ipread	iphex = { hex_open, hex_close, hex_readip };
static	FILE	*tfp = NULL;
static	int	tfd = -1;

static	int	hex_open(fname)
char	*fname;
{
	if (tfp && tfd != -1) {
		rewind(tfp);
		return tfd;
	}

	if (!strcmp(fname, "-")) {
		tfd = 0;
		tfp = stdin;
	} else {
		tfd = open(fname, O_RDONLY);
		if (tfd != -1)
			tfp = fdopen(tfd, "r");
	}
	return tfd;
}


static	int	hex_close()
{
	int	cfd = tfd;

	tfd = -1;
	return close(cfd);
}


static	int	hex_readip(buf, cnt, ifn, dir)
char	*buf, **ifn;
int	cnt, *dir;
{
	register char *s;
	struct	ip *ip;
	char	line[513];

 	ip = (struct ip *)buf;
	while (fgets(line, sizeof(line)-1, tfp)) {
		if ((s = index(line, '\n'))) {
			if (s == line)
				return (char *)ip - buf;
			*s = '\0';
		}
		if ((s = index(line, '#')))
			*s = '\0';
		if (!*line)
			continue;
		if (!(opts & OPT_BRIEF)) {
			printf("input: %s\n", line);
			fflush(stdout);
		}
		ip = (struct ip *)readhex(line, (char *)ip);
	}
	return -1;
}


static	char	*readhex(src, dst)
register char	*src, *dst;
{
	int	state = 0;
	char	c;

	while ((c = *src++)) {
		if (isspace(c)) {
			if (state) {
				dst++;
				state = 0;
			}
			continue;
		} else if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
			   (c >= 'A' && c <= 'F')) {
			c = isdigit(c) ? (c - '0') : (toupper(c) - 55);
			if (state == 0) {
				*dst = (c << 4);
				state++;
			} else {
				*dst++ |= c;
				state = 0;
			}
		} else
			break;
	}
	return dst;
}
