#	$OpenBSD: Makefile,v 1.6 2000/05/25 21:54:45 deraadt Exp $

MAN1=	fed.1
MANSUBDIR=/i386

.if ${MACHINE} == "i386"
PROG=	fed
SRCS=	fed.c select.c edit.c misc.c
LDADD=	-lcurses
.else
NOPROG=	yes
.endif

.include <bsd.prog.mk>
