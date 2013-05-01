#	$OpenBSD: Makefile,v 1.3 2010/10/18 04:10:58 deraadt Exp $

PROG=	ifconfig
SRCS=   ifconfig.c
COPTS+=	-DSMALL

LDADD=	-lutil
DPADD=	${LIBUTIL}

.PATH:  ${.CURDIR}/../../../sbin/ifconfig

CPPFLAGS+=-DINET6

# kame scopeid hack
CPPFLAGS+=-DKAME_SCOPEID

.include <bsd.prog.mk>
