#	$OpenBSD: Makefile,v 1.7 2005/11/25 20:37:20 deraadt Exp $

COPTS+=-Os
PROG=	mt
SRCS=	mt.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
LDSTATIC=-static
MAN=

.PATH:	${.CURDIR}/../../../bin/mt

.include <bsd.prog.mk>
