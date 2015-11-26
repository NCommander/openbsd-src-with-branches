#	$OpenBSD: Makefile,v 1.8 2015/10/22 12:32:33 tedu Exp $

.PATH:  ${.CURDIR}/../../usr.bin/passwd

PROG=	login_chpass
SRCS=	login_chpass.c
MAN=	login_chpass.8
CFLAGS+=-Wall

BINOWN=	root
BINGRP=	auth
BINMODE=4555
BINDIR=	/usr/libexec/auth

.include <bsd.prog.mk>
