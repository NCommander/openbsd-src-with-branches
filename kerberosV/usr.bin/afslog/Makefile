# $OpenBSD: Makefile,v 1.5 2002/05/11 00:20:18 espie Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG	= afslog

.PATH:	${KRB5SRC}/appl/afsutil

CFLAGS	+= -I.
DPADD	+= ${LIBKAFS} ${LIBKRB5} ${LIBKRB} ${LIBDES} ${LIBCRYPTO} ${LIBASN1}
LDADD	+= -lkafs -lkrb5 -lkrb -ldes -lcrypto -lasn1 ${LIBROKEN} -lcom_err

.include <bsd.prog.mk>
