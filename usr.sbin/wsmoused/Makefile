#	$OpenBSD: Makefile,v 1.2.6.1 2003/10/15 05:34:21 drahn Exp $	

.if ${MACHINE} == "i386" || ${MACHINE} == "alpha" || ${MACHINE} == "pegasos"

PROG=	wsmoused
SRCS=	wsmoused.c mouse_protocols.c

.else

NOPROG=yes

.endif

MAN=	wsmoused.8 
MANSUBDIR=	i386 alpha

.include <bsd.prog.mk>

