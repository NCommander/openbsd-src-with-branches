#	$OpenBSD: Makefile,v 1.3 2017/05/02 11:46:00 mikeb Exp $

DIR=${.CURDIR}/../../../../sys

PROG=	gmac_test
SRCS+=	aes.c gmac.c gmac_test.c
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
