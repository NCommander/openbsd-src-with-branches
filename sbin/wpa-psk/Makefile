#	$OpenBSD: Makefile,v 1.1 2008/04/15 16:29:05 damien Exp $

.PATH: ${.CURDIR}/../bioctl
CFLAGS+=-Wall -I${.CURDIR}/../bioctl

PROG=	wpa-psk
MAN=	wpa-psk.8
SRCS=	wpa-psk.c pbkdf2.c

.include <bsd.prog.mk>
