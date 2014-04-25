#	$OpenBSD: Makefile,v 1.8 2014/04/23 08:44:50 florian Exp $

PROG=	traceroute6

BINOWN=	root
BINGRP=	bin
BINMODE=4555

CPPFLAGS+=-DINET6

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual

NOMAN=	yes

.include <bsd.prog.mk>
