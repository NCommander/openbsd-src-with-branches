# $OpenBSD: Makefile,v 1.4 2002/02/06 09:13:34 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kauth
NOMAN=

SRCS=	kinit.c

.PATH:	${KRB5SRC}/kuser

DPADD+= ${LIBKRB5} ${LIBDES} ${LIBKAFS} ${LIB45} ${LIBKRB} ${LIBASN1} \
	${LIBCRYPTO} ${LIBCOM_ERR}
LDADD+=	-lkrb5 -ldes -lkafs -l45 -lkrb ${LIBROKEN} -lasn1 -lcrypto -lcom_err

.include <bsd.prog.mk>
