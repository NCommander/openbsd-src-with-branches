#	$OpenBSD: Makefile,v 1.3 2000/01/19 16:32:59 espie Exp $

.if ${MACHINE} == "amiga"
PROG=iteconfig
CFLAGS+=-I${.CURDIR}/../../sys/arch -D${MACHINE}
CFLAGS+=-W -Wall -Wstrict-prototypes
.else
NOPROG=yes
.endif

MAN=	iteconfig.8
MANSUBDIR=/amiga
.include <bsd.prog.mk>
