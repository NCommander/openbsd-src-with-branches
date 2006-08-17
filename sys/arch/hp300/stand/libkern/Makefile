#	$OpenBSD: Makefile,v 1.5 2003/06/01 17:00:34 deraadt Exp $

LIB=	kern

.PATH:	${.CURDIR}/../../../../lib/libkern
.PATH:	${.CURDIR}/../../../../lib/libkern/arch/m68k

.include "${.CURDIR}/../Makefile.inc"

CFLAGS+=-I${.CURDIR}/../include \
	-I${.CURDIR}/../../../..  -I${.CURDIR}/../../../../lib/libkern

AFLAGS=	-x assembler-with-cpp -traditional-cpp -I${.CURDIR}/../include \
	-I${.CURDIR}/../../../..  -I${.CURDIR}/../../../../lib/libkern

# kern routines
SRCS=	__main.c ashrdi3.c divdi3.c qdivrem.c strlen.S strcmp.S \
	strncmp.S umoddi3.c
SRCS+=	divsi3.S udivsi3.S modsi3.S umodsi3.S

NOPROFILE=
NOPIC=

install:

.include <bsd.lib.mk>
