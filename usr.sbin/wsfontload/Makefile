#	$OpenBSD: Makefile,v 1.1 2000/07/02 01:29:44 mickey Exp $

.if ${MACHINE} == "alpha" || ${MACHINE} == "hppa"

PROG=	wsfontload
SRCS=	wsfontload.c

.else

NOPROG=yes

.endif

MAN=	wsfontload.8

.include <bsd.prog.mk>
