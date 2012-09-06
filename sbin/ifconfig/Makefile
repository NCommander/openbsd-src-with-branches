#	$OpenBSD: Makefile,v 1.11 2010/10/18 04:10:56 deraadt Exp $

PROG=	ifconfig
SRCS=	ifconfig.c brconfig.c
MAN=	ifconfig.8

LDADD=	-lutil
DPADD=	${LIBUTIL}

CPPFLAGS+=-DINET6 -I${.CURDIR}/../bioctl

.include <bsd.prog.mk>
