#	$OpenBSD: Makefile,v 1.1 2001/04/14 04:47:40 aaron Exp $	

.if ${MACHINE} == "i386" || ${MACHINE} == "alpha"

PROG=	wsmoused
SRCS=	wsmoused.c mouse_protocols.c

.else

NOPROG=yes

.endif

MAN=	wsmoused.8 
MANSUBDIR=	i386 alpha

.include <bsd.prog.mk>

