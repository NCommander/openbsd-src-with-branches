#	$OpenBSD: Makefile,v 1.10.8.1 2003/10/10 17:04:18 drahn Exp $

.if ${MACHINE} == "i386" || ${MACHINE} == "alpha" || ${MACHINE} == "hppa" || \
    ${MACHINE} == "macppc" || ${MACHINE} == "sparc" || \
    ${MACHINE} == "sparc64" || ${MACHINE} == "pegasos"

PROG=	wsconscfg
SRCS=	wsconscfg.c

.else

NOPROG=yes

.endif

MAN=	wsconscfg.8
MANSUBDIR=	alpha hppa i386 macppc pegasos sparc sparc64

.include <bsd.prog.mk>
