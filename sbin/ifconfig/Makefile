#	$OpenBSD: Makefile,v 1.10 2009/11/22 22:00:24 claudio Exp $

.PATH:	${.CURDIR}/../bioctl

PROG=	ifconfig
SRCS=	ifconfig.c brconfig.c pbkdf2.c
MAN=	ifconfig.8

CPPFLAGS+=-DINET6 -I${.CURDIR}/../bioctl

.include <bsd.prog.mk>
