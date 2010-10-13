# $OpenBSD: Makefile,v 1.6 2003/07/14 16:14:44 mho Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kdestroy

SRCS=	kdestroy.c
SRCS+=	${PRINTVER}

.PATH:	${KRB5SRC}/kuser

DPADD+= ${LIBKRB5} ${LIBCRYPTO}
LDADD+=	-lkrb5 -lcrypto

.include <bsd.prog.mk>
