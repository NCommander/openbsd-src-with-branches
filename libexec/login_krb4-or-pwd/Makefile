#	$OpenBSD: Makefile,v 1.2 2001/06/26 05:03:27 hin Exp $

.include <bsd.own.mk>

PROG=	login_krb4-or-pwd
SRCS=	login.c login_passwd.c
MAN=	${PROG}.8
CFLAGS+=-DPASSWD -Wall -Werror -I${.CURDIR}/../login_passwd

.if (${KERBEROS:L} == "yes")
SRCS+=	login_krb4.c
DPADD+=  ${LIBKRB} ${LIBCRYPTO} ${LIBUTIL}
LDADD+=	-lkrb -lcrypto -lutil
CFLAGS+=-DKRB4
.PATH:	${.CURDIR}/../login_passwd ${.CURDIR}/../login_krb4
.else
DPADD+=  ${LIBUTIL}
LDADD+=  -lutil
.PATH:	${.CURDIR}/../login_passwd
.endif

BINOWN=	root
BINGRP= auth
BINMODE=4555
BINDIR= /usr/libexec/auth

.include <bsd.prog.mk>
