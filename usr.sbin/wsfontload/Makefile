#	$OpenBSD: Makefile,v 1.11 2007/08/01 21:35:26 deraadt Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "amd64" || \
    ${MACHINE} == "alpha" || ${MACHINE} == "hppa" || \
    ${MACHINE} == "zaurus"

PROG=	wsfontload
SRCS=	wsfontload.c

.else

NOPROG=yes

.endif

MAN=	wsfontload.8

.include <bsd.prog.mk>
