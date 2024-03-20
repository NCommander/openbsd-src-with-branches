#	$OpenBSD: Makefile,v 1.2 2022/10/02 16:40:56 jsing Exp $

PROG=	quictest
LDADD=	-lssl -lcrypto
DPADD=	${LIBSSL} ${LIBCRYPTO}

WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

REGRESS_TARGETS= \
	regress-quictest

regress-quictest: ${PROG}
	./quictest \
	    ${.CURDIR}/../../libssl/certs/server1-rsa.pem \
	    ${.CURDIR}/../../libssl/certs/server1-rsa-chain.pem \
	    ${.CURDIR}/../../libssl/certs/ca-root-rsa.pem

.include <bsd.regress.mk>
