#	$OpenBSD: Makefile,v 1.3 2000/05/31 19:52:59 mickey Exp $

MAN=	mkboot.8
MANSUBDIR=/hppa

.if ${MACHINE} == "hppa"
PROG=	mkboot
CC=	${HOSTCC}
CFLAGS=-g -O
LDSTATIC=	-static
.else
NOPROG=
.endif

.include <bsd.prog.mk>
