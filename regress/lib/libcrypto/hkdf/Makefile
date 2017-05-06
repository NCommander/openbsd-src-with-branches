#	$OpenBSD: Makefile,v 1.5 2015/09/14 01:45:03 doug Exp $

PROG=	hkdf_test
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL

.include <bsd.regress.mk>
