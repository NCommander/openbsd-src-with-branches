#	$OpenBSD: Makefile,v 1.7 2013/04/15 15:51:47 deraadt Exp $

PROG=	traceroute6

BINOWN=	root
BINGRP=	bin
BINMODE=4555

CPPFLAGS+=-DINET6

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual

MAN=	traceroute6.8

.include <bsd.prog.mk>
