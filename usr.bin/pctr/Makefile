#	$OpenBSD: Makefile,v 1.4 2000/05/25 19:09:08 deraadt Exp $

MAN=pctr.1
MANSUBDIR=/i386

.if ${MACHINE} == "i386"
PROG=	pctr
.else
NOPROG=
.endif

.include <bsd.prog.mk>
