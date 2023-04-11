#	$OpenBSD$

PROG=	sm3test
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

.include <bsd.regress.mk>
