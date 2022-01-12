#	$OpenBSD: Makefile,v 1.3 2014/07/08 15:53:52 jsing Exp $

PROG=	dhtest
LDADD=	${CRYPTO_INT} # XXX link dynamically after bump
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -DLIBRESSL_CRYPTO_INTERNAL -Werror

.include <bsd.regress.mk>
