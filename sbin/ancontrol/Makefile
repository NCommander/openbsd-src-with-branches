#	$OpenBSD: Makefile,v 1.5 2001/09/01 16:21:16 drahn Exp $

.if (${MACHINE} == "i386") || (${MACHINE_ARCH} == "powerpc") || \
    (${MACHINE} == "alpha")
PROG=	ancontrol
CPPFLAGS+=-DANCACHE
.else
NOPROG=
.endif

MAN=	ancontrol.8

.include <bsd.prog.mk>
