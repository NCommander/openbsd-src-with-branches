# $OpenBSD: Makefile,v 1.7 2003/05/14 06:56:02 mho Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kinit
MLINKS=	kinit.1 kauth.1

SRCS=	kinit.c
SRCS+=	${PRINTVER}

.PATH:	${KRB5SRC}/kuser

DPADD+=	${LIBKRB5} ${LIBDES} ${LIBCRYPTO}
LDADD+=	-lkrb5 -ldes -lcrypto

.include <bsd.prog.mk>
