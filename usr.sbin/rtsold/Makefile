#	$OpenBSD: Makefile,v 1.2 2000/01/05 01:54:50 itojun Exp $

PROG=	rtsold
SRCS=	rtsold.c rtsol.c if.c probe.c dump.c

CPPFLAGS+=-DINET6 -DHAVE_ARC4RANDOM -DHAVE_GETIFADDRS

MAN=	rtsold.8
MLINKS=	rtsold.8 rtsol.8

.include <bsd.prog.mk>
