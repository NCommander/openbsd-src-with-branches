# $OpenBSD: Makefile,v 1.4 2003/07/14 16:14:45 mho Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	string2key
MAN=	${PROG}.8

SRCS=	string2key.c
SRCS+=	${PRINTVER}
SRCS+=	estrdup.c

.PATH:	${KRB5SRC}/kdc
.PATH:	${ROKENSRC}

CFLAGS+=-I${KRB5SRC}/kdc -I${KRB5SRC}/lib/krb5

DPADD+=	${LIBKRB5} ${LIBCRYPTO}
LDADD+=	-lkrb5 -lcrypto

.include <bsd.prog.mk>
