#	$OpenBSD: Makefile,v 1.2 1997/09/21 11:44:33 deraadt Exp $

.if ${MACHINE} == "amiga"
PROG=videomode
CFLAGS+=-I${.CURDIR}/../../sys/arch -D${MACHINE}
.else
NOPROG=yes
.endif

NOMAN=	noman
MANSUBDIR=/amiga

.include <bsd.prog.mk>
