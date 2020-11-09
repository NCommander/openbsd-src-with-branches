# $OpenBSD$

SUBDIR += libressl
.if exists(/usr/local/bin/eopenssl11)
SUBDIR += openssl11
.endif

.include <bsd.subdir.mk>
.include <bsd.regress.mk>
