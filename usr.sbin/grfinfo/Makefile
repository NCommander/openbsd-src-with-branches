#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:43:40 deraadt Exp $

.if ${MACHINE} == "hp300"
PROG=	grfinfo
CFLAGS+= -I${.CURDIR}/../../sys/arch/hp300
.else
NOPROG=yes
.endif

MAN=grfinfo.1
MANSUBDIR=hp300

.include <bsd.prog.mk>
