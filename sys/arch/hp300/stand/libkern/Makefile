#	$OpenBSD: Makefile,v 1.2 1997/01/18 03:18:18 downsj Exp $

LIB=	kern

.PATH:	${.CURDIR}/../../../../lib/libkern

CFLAGS= -O2 -msoft-float -I${.CURDIR}/../include \
	-I${.CURDIR}/../../../..  -I${.CURDIR}/../../../../lib/libkern

# kern routines
SRCS=	__main.c ashrdi3.c divdi3.c qdivrem.c strlen.c strcat.c strcmp.c \
	strcpy.c strncmp.c umoddi3.c

${OBJS}:	${.CURDIR}/../Makefile.inc

NOPROFILE=
NOPIC=

install:

.include <bsd.lib.mk>
