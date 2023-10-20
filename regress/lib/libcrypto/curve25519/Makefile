#	$OpenBSD: Makefile,v 1.1 2016/11/05 15:09:02 jsing Exp $

PROGS += ed25519test
PROGS += x25519test

.for p in ${PROGS}
REGRESS_TARGETS += run-$p
.endfor

LDADD = ${CRYPTO_INT}
DPADD = ${LIBCRYPTO}

WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

.for p in ${PROGS}
run-$p: $p
	@echo '\n======== $@ ========'
	./$p

.PHONY: run-$p
.endfor

.include <bsd.regress.mk>
