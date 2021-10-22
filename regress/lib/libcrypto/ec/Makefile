#	$OpenBSD: Makefile,v 1.4 2021/04/20 17:09:45 tb Exp $

PROGS +=		ectest
PROGS +=		ec_point_conversion

.for t in ${PROGS}
REGRESS_TARGETS +=	run-$t
.endfor

LDADD =			${CRYPTO_INT}
DPADD =			${LIBCRYPTO}
WARNINGS =		Yes
CFLAGS +=		-DLIBRESSL_INTERNAL -Wall -Wundef -Werror

CLEANFILES +=		${PROGS}

.for t in ${PROGS}
run-$t: $t
	./$t
.endfor

.include <bsd.regress.mk>
