# $OpenBSD: Makefile,v 1.5 2003/05/17 01:31:21 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kdestroy

SRCS=	kdestroy.c
SRCS+=	${PRINTVER}

.PATH:	${KRB5SRC}/kuser

DPADD+= ${LIBKRB5} ${LIBDES} ${LIBCRYPTO}
LDADD+=	-lkrb5 -ldes -lcrypto

.include <bsd.prog.mk>
