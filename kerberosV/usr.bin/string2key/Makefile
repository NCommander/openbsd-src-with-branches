# $OpenBSD: Makefile,v 1.2 2001/06/23 12:48:44 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	string2key
MAN=	${PROG}.8

.PATH:	${KRB5SRC}/kdc

CFLAGS+=-I${KRB5SRC}/kdc -I${KRB5SRC}/lib/krb5
LDADD+=	-lkrb5 -ldes -lkafs -lkrb ${LIBROKEN} -lasn1 -lcrypto -lcom_err

.include <bsd.prog.mk>