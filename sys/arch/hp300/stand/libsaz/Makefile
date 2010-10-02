#	$OpenBSD: Makefile,v 1.3 2006/08/17 06:31:10 miod Exp $

CFLAGS+=-D__INTERNAL_LIBSA_CREAD

# io routines
SRCS+=	cread.c

.include "${.CURDIR}/../libsa/Makefile"
.PATH:	${.CURDIR}/../libsa

LIB=	saz
