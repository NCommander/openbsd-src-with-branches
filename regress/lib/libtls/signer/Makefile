#	$OpenBSD: Makefile,v 1.1 2018/02/08 10:06:52 jsing Exp $

PROG=	signertest
LDADD=	-lcrypto -lssl -ltls
DPADD=	${LIBCRYPTO} ${LIBSSL} ${LIBTLS}

WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Wall -Wundef -Werror

REGRESS_TARGETS= \
	regress-signertest

regress-signertest: ${PROG}
	./signertest \
	    ${.CURDIR}/../../libssl/certs

.include <bsd.regress.mk>
