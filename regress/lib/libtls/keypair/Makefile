#	$OpenBSD: Makefile,v 1.1 2018/02/08 10:06:52 jsing Exp $

PROG=	keypairtest
LDADD=	-lcrypto -lssl ${TLS_INT}
DPADD=	${LIBCRYPTO} ${LIBSSL} ${LIBTLS}

WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Wall -Wundef -Werror
CFLAGS+=	-I${.CURDIR}/../../../../lib/libtls

REGRESS_TARGETS= \
	regress-keypairtest

regress-keypairtest: ${PROG}
	./keypairtest \
	    ${.CURDIR}/../../libssl/certs/ca-root-rsa.pem \
	    ${.CURDIR}/../../libssl/certs/server1-rsa.pem \
	    ${.CURDIR}/../../libssl/certs/server1-rsa.pem

.include <bsd.regress.mk>
