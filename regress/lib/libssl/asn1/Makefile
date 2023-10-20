#	$OpenBSD: Makefile,v 1.1 2014/07/13 16:03:54 jsing Exp $

PROG=	asn1test
LDADD=	-lcrypto -lssl
DPADD=	${LIBCRYPTO} ${LIBSSL}

WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror
CFLAGS+=	-I${.CURDIR}/../../../../lib/libssl

.include <bsd.regress.mk>
