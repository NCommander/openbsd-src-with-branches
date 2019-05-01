#	$OpenBSD$

.include "../../Makefile.inc"

PROG=		bnaddsub
LDADD=		${CRYPTO_INT}
DPADD=		${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-Werror

.include <bsd.regress.mk>
