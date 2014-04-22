#	$OpenBSD: Makefile,v 1.18 2014/04/15 20:55:42 miod Exp $

.include <bsd.own.mk>

PROG=	login_krb5-or-pwd
SRCS=	login.c login_passwd.c pwd_gensalt.c
MAN=	${PROG}.8
CFLAGS+=-DPASSWD -Wall -I${.CURDIR}/../login_passwd

DPADD+=  ${LIBUTIL}
LDADD+=  -lutil

.PATH:	${.CURDIR}/../login_passwd ${.CURDIR}/../../usr.bin/passwd

BINOWN=	root
BINGRP= auth
BINMODE=4555
BINDIR= /usr/libexec/auth

.include <bsd.prog.mk>
