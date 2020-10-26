#	$OpenBSD: Makefile,v 1.1 2019/01/19 02:57:04 jsing Exp $

PROG=		record_layer_test
LDADD=		${SSL_INT} -lcrypto
DPADD=		${LIBSSL} ${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Wall -Wundef -Werror
CFLAGS+=	-I${.CURDIR}/../../../../lib/libssl

.include <bsd.regress.mk>
