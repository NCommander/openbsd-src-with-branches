#	$OpenBSD: Makefile,v 1.4 1999/01/13 07:26:04 niklas Exp $

.if ${MACHINE} == "i386"
PROG=	ispcvt
.else
NOPROG=yes
.endif

MAN8=	ispcvt.${MAN8EXT}
MANSUBDIR=i386

.include <bsd.prog.mk>
