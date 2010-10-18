#	$OpenBSD: Makefile,v 1.2 2009/10/28 07:36:49 deraadt Exp $

PROG=	ifconfig
SRCS=   ifconfig.c pbkdf2.c
COPTS+=	-DSMALL
.PATH:  ${.CURDIR}/../../../sbin/ifconfig ${.CURDIR}/../../../sbin/bioctl

CPPFLAGS+=-DINET6 -I${.CURDIR}/../../../sbin/bioctl

# kame scopeid hack
CPPFLAGS+=-DKAME_SCOPEID

.include <bsd.prog.mk>
