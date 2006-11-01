#	$OpenBSD: Makefile,v 1.7 2004/01/26 19:48:33 miod Exp $

LIB=prom

NOPIC=
NOPROFILE=

S=${.CURDIR}/../../../..
DIR_SA=$S/lib/libsa

CFLAGS+=-I${.CURDIR}/../../include -I${DIR_SA}

SRCS=	getc.c getenaddr.c halt.c putc.c

install:

.include <bsd.lib.mk>
