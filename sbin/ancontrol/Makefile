#	$OpenBSD: Makefile,v 1.3 2001/06/23 19:58:19 mickey Exp $

.if (${MACHINE} == "i386") || (${MACHINE} == "powerpc") || \
    (${MACHINE} == "alpha")
PROG=	ancontrol
CFLAGS+=-DANCACHE
.else
NOPROG=
.endif

MAN=	ancontrol.8

.include <bsd.prog.mk>
