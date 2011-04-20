# $OpenBSD: Makefile,v 1.3 2003/07/14 16:14:44 mho Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG	= afslog

.PATH:	${KRB5SRC}/appl/afsutil

SRCS+=	afslog.c
SRCS+=	${PRINTVER}

DPADD	+= ${LIBKRB5} ${LIBCRYPTO}
LDADD	+= -lkrb5 -lcrypto

.include <bsd.prog.mk>
