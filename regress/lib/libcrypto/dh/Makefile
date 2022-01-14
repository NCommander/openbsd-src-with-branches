#	$OpenBSD: Makefile,v 1.4 2022/01/12 08:56:49 tb Exp $

PROG=	dhtest
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

.include <bsd.regress.mk>
