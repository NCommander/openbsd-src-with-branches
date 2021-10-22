#	$OpenBSD: Makefile,v 1.1 2020/10/14 15:49:14 jsing Exp $

PROG=		dtlstest
LDADD=		${SSL_INT} -lcrypto
DPADD=		${LIBSSL} ${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror
CFLAGS+=	-I${.CURDIR}/../../../../lib/libssl

REGRESS_TARGETS= \
	regress-dtlstest

regress-dtlstest: ${PROG}
	./dtlstest \
	    ${.CURDIR}/../../libssl/certs/server.pem \
	    ${.CURDIR}/../../libssl/certs/server.pem \
	    ${.CURDIR}/../../libssl/certs/ca.pem

.include <bsd.regress.mk>
