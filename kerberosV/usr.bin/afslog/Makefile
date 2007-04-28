# $OpenBSD: Makefile,v 1.2 2003/05/17 01:31:21 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG	= afslog

.PATH:	${KRB5SRC}/appl/afsutil

SRCS+=	afslog.c
SRCS+=	${PRINTVER}

DPADD	+= ${LIBKRB5} ${LIBDES} ${LIBCRYPTO}
LDADD	+= -lkrb5 -ldes -lcrypto

.include <bsd.prog.mk>
