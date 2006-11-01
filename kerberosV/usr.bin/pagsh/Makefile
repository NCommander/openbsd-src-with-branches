# $OpenBSD: Makefile,v 1.5 2003/05/17 01:31:21 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	pagsh

SRCS=	pagsh.c
SRCS+=	${PRINTVER}
SRCS+=	esetenv.c k_getpwuid.c

.PATH:	${KRB5SRC}/appl/afsutil
.PATH:	${ROKENSRC}

DPADD+=	${LIBKRB5} ${LIBDES} ${LIBCRYPTO}
LDADD+=	-lkrb5 -ldes -lcrypto

.include <bsd.prog.mk>
