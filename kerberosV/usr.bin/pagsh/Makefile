# $OpenBSD: Makefile,v 1.2 2001/06/23 12:48:44 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	pagsh

.PATH:	${KRB5SRC}/appl/afsutil
NOMAN=

CFLAGS+=-I${KRB5SRC}/lib/roken
DPADD+=	${LIBKRB5} ${LIBDES} ${LIBKAFS} ${LIBKRB} ${LIBASN1} ${LIBCRYPTO} \
	${LIBCOM_ERR}
LDADD+=	-lkrb5 -ldes -lkafs -lkrb ${LIBROKEN} -lasn1 -lcrypto -lcom_err

.include <bsd.prog.mk>
