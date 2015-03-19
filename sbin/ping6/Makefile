#	$OpenBSD: Makefile,v 1.10 2013/04/16 16:51:36 deraadt Exp $

PROG=	ping6

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual

MAN=	ping6.8

LDADD=	-lm
DPADD=	${LIBM}

BINOWN=	root
BINGRP=	bin
BINMODE=4555

.include <bsd.prog.mk>
