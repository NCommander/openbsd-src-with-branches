# $OpenBSD: Makefile,v 1.3 2022/01/14 09:33:46 tb Exp $

PROG=		cttest
LDADD=		-lcrypto
DPADD=		${LIBCRYPTO}

WARNINGS=	Yes
CFLAGS+=	-Wundef -Werror
CFLAGS+=	-DLIBRESSL_CRYPTO_INTERNAL -DLIBRESSL_INTERNAL
CFLAGS+=	-I$(BSDSRCDIR)/lib/libcrypto

run-regress-cttest: cttest
	./cttest ${.CURDIR}/../../libcrypto/ct/

.include <bsd.regress.mk>
