#	$OpenBSD: Makefile,v 1.4 1999/01/13 07:26:03 niklas Exp $

.if ${MACHINE} == "i386"
PROG=	fed
SRCS=	fed.c select.c edit.c misc.c
LDADD=	-lcurses
.else
NOPROG=	yes
.endif
MAN=	fed.1
MANSUBDIR=i386

.include <bsd.prog.mk>
