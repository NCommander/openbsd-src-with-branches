#	$OpenBSD: Makefile,v 1.1 1999/07/11 16:31:08 niklas Exp $
#	From: Makefile,v 1.1 1999/05/05 07:37:06 wpaul Exp $

.if ${MACHINE} == "i386"
PROG=	wicontrol
SRCS=	wicontrol.c

CFLAGS+= -Wall
.else
NOPROG=yes
.endif

MAN=	wicontrol.8
MANSUBDIR=i386

.include <bsd.prog.mk>
