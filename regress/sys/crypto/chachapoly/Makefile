#	$OpenBSD: Makefile,v 1.1 2015/11/03 01:46:08 mikeb Exp $

DIR=${.CURDIR}/../../../../sys

PROG=		chachapoly_test
SRCS+=		poly1305.c chachapoly.c chachapoly_test.c
CDIAGFLAGS=	-Wall
CDIAGFLAGS+=	-Werror
CDIAGFLAGS+=	-Wpointer-arith
CDIAGFLAGS+=	-Wno-uninitialized
CDIAGFLAGS+=	-Wstrict-prototypes
CDIAGFLAGS+=	-Wmissing-prototypes
CDIAGFLAGS+=	-Wunused
CDIAGFLAGS+=	-Wsign-compare
#CDIAGFLAGS+=	-Wshadow

CFLAGS+=	-I${DIR}

.PATH:	${DIR}/crypto

.include <bsd.regress.mk>
