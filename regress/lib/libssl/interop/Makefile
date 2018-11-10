# $OpenBSD: Makefile,v 1.3 2018/11/09 06:30:41 bluhm Exp $

SUBDIR =	libressl openssl openssl11
# the above binaries must have been built before we can continue
SUBDIR +=	session
SUBDIR +=	cert

.include <bsd.subdir.mk>
