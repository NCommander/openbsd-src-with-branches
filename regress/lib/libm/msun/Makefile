# $OpenBSD$

TESTS =
TESTS +=	conj_test
TESTS +=	fenv_test
TESTS +=	lrint_test

PROGS=	${TESTS}
LDADD=	-lm
DPADD=	${LIBM}

.for t in ${TESTS}
REGRESS_TARGETS+=	run-$t
run-$t: $t
	@echo "\n======== $@ ========"
	./$t
.endfor

.include <bsd.regress.mk>
