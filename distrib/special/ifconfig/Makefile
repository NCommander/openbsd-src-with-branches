#	$OpenBSD: Makefile,v 1.4 2012/09/07 00:20:21 deraadt Exp $

PROG=	ifconfig
SRCS=   ifconfig.c
COPTS+=	-DSMALL

LDADD=	-lutil
DPADD=	${LIBUTIL}

.PATH:  ${.CURDIR}/../../../sbin/ifconfig

.include <bsd.prog.mk>
