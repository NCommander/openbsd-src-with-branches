#	$OpenBSD: Makefile,v 1.1 2010/02/14 22:39:33 miod Exp $

NOMAN=
#MAN=		boot.8
#MLINKS=	boot.8 boot.conf.5
.if ${MACHINE} == "loongson"
PROG=		boot
.include	"${.CURDIR}/../boot/Makefile.inc"
.else
NOPROG=
.endif

.include <bsd.prog.mk>
