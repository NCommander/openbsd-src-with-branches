#	$OpenBSD: Makefile,v 1.4 2000/05/25 21:54:45 deraadt Exp $

MAN1=cursor.1
MANSUBDIR=/i386

.if ${MACHINE} == "i386"
PROG=	cursor
.else
NOPROG=yes
.endif

.include <bsd.prog.mk>
