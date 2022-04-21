#	$OpenBSD: Makefile,v 1.7 2018/06/13 14:54:42 reyk Exp $

PROG=	login_passwd
MAN=	login_passwd.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
CFLAGS+=-Wall

BINOWN=	root
BINGRP=	auth
BINMODE=4555
BINDIR=	/usr/libexec/auth

.include <bsd.prog.mk>
