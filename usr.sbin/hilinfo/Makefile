#	$OpenBSD: Makefile,v 1.2 1997/09/21 11:43:43 deraadt Exp $

.if ${MACHINE}=="hp300"
PROG=	hilinfo
CFLAGS+= -I${.CURDIR}/../../sys/arch/hp300
.else
NOPROG=yes
.endif

NOMAN=
MANSUBDIR=hp300

.include <bsd.prog.mk>
