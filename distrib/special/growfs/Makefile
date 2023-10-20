# $OpenBSD: Makefile,v 1.8 2015/11/23 18:35:18 mmcc Exp $

PROG=	growfs
SRCS=	growfs.c

DPADD=	${LIBUTIL}
LDADD=	-lutil

.PATH:	${.CURDIR}/../../../sbin/growfs
.include <bsd.prog.mk>
