#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:43:18 deraadt Exp $

.if ${MACHINE} == "i386"
SRCS=	apmd.c apmsubr.c

CFLAGS+=	-g  -O2 -Wmissing-prototypes -Wall
PROG=	apmd
.else
NOPROG=yes
.endif

MAN=	apmd.8
MANSUBDIR=i386

.include <bsd.prog.mk>
