#	$OpenBSD: Makefile,v 1.1 2000/07/02 01:25:24 mickey Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "hppa"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8

.include <bsd.prog.mk>
