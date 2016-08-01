#	$OpenBSD$

TEST_ENV=      "MALLOC_OPTIONS=AFGJPRX"

PROG=test_bitmap
SRCS=tests.c
REGRESS_TARGETS=run-regress-${PROG}

run-regress-${PROG}: ${PROG}
	env ${TEST_ENV} ./${PROG}

.include <bsd.regress.mk>
