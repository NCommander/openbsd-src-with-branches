#	$OpenBSD: Makefile,v 1.1 2022/01/30 18:38:41 jsing Exp $

PROG=	signertest
LDADD=	-lcrypto -lssl
LDADD+=	-Wl,-Bstatic -ltls -Wl,-Bdynamic
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
