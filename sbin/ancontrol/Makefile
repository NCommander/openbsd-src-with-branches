#	$OpenBSD: Makefile,v 1.1 2000/04/03 01:08:09 mickey Exp $

.if ${MACHINE}=="i386"
PROG=	ancontrol
CFLAGS+=-DANCACHE
.else
NOPROG=yes
.endif

MAN=	ancontrol.8
MANSUBDIR=i386
.include <bsd.prog.mk>
