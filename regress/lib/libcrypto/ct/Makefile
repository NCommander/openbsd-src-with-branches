# $OpenBSD: Makefile,v 1.1 2017/07/16 18:18:10 jsing Exp $

PROG=		cttest
LDADD=		${CRYPTO_INT}
DPADD=		${LIBCRYPTO}

WARNINGS=	Yes
CFLAGS+=	-Wundef -Werror
CFLAGS+=	-DLIBRESSL_CRYPTO_INTERNAL -DLIBRESSL_INTERNAL
CFLAGS+=	-I$(BSDSRCDIR)/lib/libcrypto

REGRESS_TARGETS= \
	regress-cttest

regress-cttest: ${PROG}
	./cttest \
	    ${.CURDIR}/../../libcrypto/ct/libressl.org.crt

.include <bsd.regress.mk>
