#	$OpenBSD: Makefile,v 1.2 2022/03/24 15:58:57 tb Exp $

PROG=	signertest
LDADD=	-lcrypto -lssl ${TLS_INT}
DPADD=	${LIBCRYPTO} ${LIBSSL} ${LIBTLS}

WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Wall -Wundef -Werror
CFLAGS+=	-I${.CURDIR}/../../../../lib/libtls/

REGRESS_TARGETS= \
	regress-signertest

regress-signertest: ${PROG}
	./signertest \
	    ${.CURDIR}/../../libssl/certs

.include <bsd.regress.mk>
