# $OpenBSD: Makefile,v 1.1 2015/10/15 19:43:30 tedu Exp $

PROG=	rebound
SRCS=	rebound.c randomid.c https.c
LDADD=	-ltls
DPADD=	${LIBTLS}

CFLAGS+=-Wall

MAN=	rebound.8

.include <bsd.prog.mk>
