#	$OpenBSD: Makefile,v 1.2 2001/06/26 05:03:28 hin Exp $

PROG=	login_passwd
MAN=	login_passwd.8
SRCS=	login.c login_passwd.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
CFLAGS+=-Wall -DPASSWD

BINOWN=	root
BINGRP=	_shadow
BINMODE=2555
BINDIR=	/usr/libexec/auth

.include <bsd.prog.mk>
