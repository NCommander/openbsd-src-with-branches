#       $OpenBSD$

PROG=	qsort_test
CFLAGS+=-Wall
LDADD+=	-lm
DPADD+=	${LIBM}

qsort-regress: ${PROG}
	./${PROG}

REGRESS_TARGETS=qsort-regress
.PHONY: ${REGRESS_TARGETS}

.include <bsd.regress.mk>
