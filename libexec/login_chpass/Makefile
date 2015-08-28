#	$OpenBSD: Makefile,v 1.6 2010/10/13 08:14:22 jsg Exp $


PROG=	login_chpass
SRCS=	login_chpass.c
MAN=	login_chpass.8

.PATH:  ${.CURDIR}/../../usr.bin/passwd

.include <bsd.own.mk>	# For YP

CFLAGS+=-Wall

.if (${YP:L} == "yes")
CFLAGS+=-DYP
SRCS+= yp_passwd.c pwd_check.c pwd_gensalt.c
DPADD+= ${LIBRPCSVC} ${LIBUTIL}
LDADD+= -lrpcsvc -lutil
.endif

BINOWN=	root
BINGRP=	auth
BINMODE=4555
BINDIR=	/usr/libexec/auth

.include <bsd.prog.mk>
