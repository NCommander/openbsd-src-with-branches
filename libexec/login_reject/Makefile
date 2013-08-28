#	$OpenBSD: Makefile,v 1.1 2000/12/12 02:34:43 millert Exp $

PROG=	login_reject
SRCS=	login_reject.c pwd_gensalt.c
MAN=	login_reject.8
CFLAGS+=-Wall
.PATH:  ${.CURDIR}/../../usr.bin/passwd

BINOWN=	root
BINGRP=	auth
BINMODE=555
BINDIR= /usr/libexec/auth

.include <bsd.prog.mk>
