#	$OpenBSD$

DIR=${.CURDIR}/../../../../sys

PROG=	cmac_test
SRCS+=	rijndael.c cmac.c cmac_test.c
CDIAGFLAGS=	-Wall
CDIAGFLAGS+=	-Werror
CDIAGFLAGS+=	-Wpointer-arith
CDIAGFLAGS+=	-Wno-uninitialized
CDIAGFLAGS+=	-Wstrict-prototypes
CDIAGFLAGS+=	-Wmissing-prototypes
CDIAGFLAGS+=	-Wunused
CDIAGFLAGS+=	-Wsign-compare
CDIAGFLAGS+=	-Wbounded
CDIAGFLAGS+=	-Wshadow

REGRESS_TARGETS=	run-regress-${PROG}

CFLAGS+=	-I${DIR}

.PATH:	${DIR}/crypto/

run-regress-${PROG}: ${PROG}
	./${PROG} | diff - ${PROG}.txt

.include <bsd.regress.mk>
