#	$OpenBSD: Makefile,v 1.2 1999/01/13 07:26:08 niklas Exp $

.if ${MACHINE} == "i386"
PROG=		vttest
CFLAGS+=	-traditional -DUSEMYSTTY
SRCS=		main.c esc.c
.else
NOPROG=yes
.endif

MAN=		vttest.1
MANSUBDIR=	i386

.include <bsd.prog.mk>
