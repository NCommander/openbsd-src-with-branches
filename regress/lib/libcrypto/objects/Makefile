# $OpenBSD$

PROG=		objectstest
LDADD=		-lcrypto
DPADD=		${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror

.include <bsd.regress.mk>
