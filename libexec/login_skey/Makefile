#	$OpenBSD$

PROG=	login_skey
MAN=	login_skey.8
DPADD=	${LIBSKEY}
LDADD=	-lskey
CFLAGS+=-Wall

BINOWN=	root
BINGRP=	auth
BINMODE=4555
BINDIR=	/usr/libexec/auth

.include <bsd.prog.mk>
