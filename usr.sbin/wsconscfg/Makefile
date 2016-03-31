#	$OpenBSD: Makefile,v 1.36 2016/03/09 16:28:50 deraadt Exp $

.if ${MACHINE} != "hppa64" && ${MACHINE} != "octeon"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8

.include <bsd.prog.mk>
