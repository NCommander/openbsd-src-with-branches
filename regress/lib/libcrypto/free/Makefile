#	$OpenBSD: Makefile,v 1.2 2015/09/29 04:54:23 beck Exp $

TESTS = \
	freenull

REGRESS_TARGETS= all_tests

LDADD=		-lcrypto
DPADD=		${LIBCRYPTO} ${LIBSSL}
LDFLAGS+=	-lcrypto
CFLAGS+=	-DLIBRESSL_INTERNAL

CLEANFILES+= ${TESTS}

all_tests: ${TESTS}
	@for test in $>; do \
		./$$test; \
	done

.include <bsd.regress.mk>
