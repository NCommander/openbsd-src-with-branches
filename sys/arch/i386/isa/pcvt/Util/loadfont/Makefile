#	$OpenBSD: Makefile,v 1.4 2000/05/25 21:54:51 deraadt Exp $

MAN1=	loadfont.1
MANSUBDIR=/i386

.if ${MACHINE} == "i386"
PROG=	loadfont
.else
NOPROG=	yes
.endif

.include <bsd.prog.mk>
