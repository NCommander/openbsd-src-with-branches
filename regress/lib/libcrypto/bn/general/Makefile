#	$OpenBSD: Makefile,v 1.4 2017/01/21 09:38:58 beck Exp $

.include "../../Makefile.inc"

PROGS +=	bntest
PROGS +=	bn_to_string

LDADD =		${CRYPTO_INT}
DPADD =		${LIBCRYPTO}
WARNINGS =	Yes
CFLAGS +=	-Werror

.for p in ${PROGS}
REGRESS_TARGETS += run-$p

run-$p: $p
	./$p

.PHONY: run-$p
.endfor

.include <bsd.regress.mk>
