#	$OpenBSD: Makefile,v 1.2 2008/09/02 12:55:44 djm Exp $

DIR=${.CURDIR}/../../../../sys

PROG=	gmac_test
SRCS+=	rijndael.c gmac.c gmac_test.c
CDIAGFLAGS=	-Wall
CDIAGFLAGS+=	-Werror
CDIAGFLAGS+=	-Wpointer-arith
CDIAGFLAGS+=	-Wno-uninitialized
CDIAGFLAGS+=	-Wstrict-prototypes
CDIAGFLAGS+=	-Wmissing-prototypes
CDIAGFLAGS+=	-Wunused
CDIAGFLAGS+=	-Wsign-compare
CDIAGFLAGS+=	-Wbounded
#CDIAGFLAGS+=	-Wshadow

REGRESS_TARGETS=	run-regress-${PROG}

CFLAGS+=	-I${DIR}

.PATH:	${DIR}/crypto

run-regress-${PROG}: ${PROG}
	./${PROG}

.include <bsd.regress.mk>
