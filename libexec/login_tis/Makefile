#	$OpenBSD$

PROG=	login_tis
MAN=	login_tis.8
CFLAGS+=-Wall
LDADD+=	-ldes
DPADD+=	${LIBDES}

BINOWN=	root
BINGRP=	auth
BINMODE=4555
BINDIR=	/usr/libexec/auth

.include <bsd.prog.mk>
