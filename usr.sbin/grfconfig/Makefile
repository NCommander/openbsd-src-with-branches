#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:43:39 deraadt Exp $

.if ${MACHINE} == "amiga"
PROG=		grfconfig
CFLAGS+=	-I${.CURDIR}/../../sys/arch -D${MACHINE}
.else
NOPROG=yes
.endif

MAN=	grfconfig.8
MANSUBDIR=/amiga

.include <bsd.prog.mk>
