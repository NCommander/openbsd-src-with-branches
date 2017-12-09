#	$OpenBSD: Makefile,v 1.3 2017/01/09 12:34:03 jsing Exp $

PROG=	configtest
LDADD=	-lcrypto -lssl -ltls
DPADD=	${LIBCRYPTO} ${LIBSSL} ${LIBTLS}

WARNINGS=	Yes
CFLAGS+=	-Werror

.include <bsd.regress.mk>
