#	$OpenBSD: Makefile,v 1.6 2022/06/02 11:48:36 tb Exp $

PROG =		rfc3779
LDADD =		-lcrypto
DPADD =		${LIBCRYPTO}
WARNINGS =	Yes
CFLAGS +=	-Werror

.include <bsd.regress.mk>
