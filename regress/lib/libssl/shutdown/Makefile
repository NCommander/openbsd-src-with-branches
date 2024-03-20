#	$OpenBSD: Makefile,v 1.1 2024/01/19 08:29:08 jsing Exp $

PROG=	shutdowntest
LDADD=	-lssl -lcrypto
DPADD=	${LIBSSL} ${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

REGRESS_TARGETS= \
	regress-shutdowntest

regress-shutdowntest: ${PROG}
	./shutdowntest \
	    ${.CURDIR}/../../libssl/certs/server1-rsa.pem \
	    ${.CURDIR}/../../libssl/certs/server1-rsa-chain.pem \
	    ${.CURDIR}/../../libssl/certs/ca-root-rsa.pem

.include <bsd.regress.mk>
