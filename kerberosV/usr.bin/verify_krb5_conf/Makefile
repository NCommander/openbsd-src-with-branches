# $OpenBSD: Makefile,v 1.3 2003/05/17 01:31:21 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	verify_krb5_conf
MAN=	${PROG}.8

SRCS=	verify_krb5_conf.c
SRCS+=	${PRINTVER}
SRCS+=	parse_bytes.c

.PATH:	${KRB5SRC}/lib/krb5
.PATH:	${ROKENSRC}

DPADD+= ${LIBKRB5} ${LIBDES} ${LIBCRYPTO}
LDADD+=	-lkrb5 -ldes -lcrypto

.include <bsd.prog.mk>
