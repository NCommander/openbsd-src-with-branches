#	$OpenBSD$

PROG=	login_radius
SRCS=	login_radius.c raddauth.c
MAN=	login_radius.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
CFLAGS+=-Wall

BINOWN=	root
BINGRP=	auth
BINMODE=4555
BINDIR=	/usr/libexec/auth

.include <bsd.prog.mk>
