#	$OpenBSD: Makefile,v 1.2 2001/06/25 22:02:13 hin Exp $

PROG=	login_krb5-or-pwd
SRCS=	login_krb5.c login_passwd.c login.c
MAN=	${PROG}.8
LDADD=  -lkrb5 -lasn1 -lcrypto -lcom_err -lkrb -lutil
CFLAGS+=-DKRB524 -DKRB5 -DPASSWD -Wall -Werror -I${.CURDIR}/../login_passwd
.PATH:	${.CURDIR}/../login_passwd ${.CURDIR}/../login_krb5

BINOWN=	root
BINGRP= auth
BINMODE=4555
BINDIR= /usr/libexec/auth

.include <bsd.prog.mk>
