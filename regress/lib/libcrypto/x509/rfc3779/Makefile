#	$OpenBSD: Makefile,v 1.1 2018/11/06 06:55:27 tb Exp $

.include "../../Makefile.inc"

PROG=	rfc3779
LDADD=	${CRYPTO_INT}
DPADD=	${LIBCRYPTO}
WARNINGS=	Yes
CFLAGS+=	-DLIBRESSL_INTERNAL -Werror -g -O0

.include <bsd.regress.mk>
