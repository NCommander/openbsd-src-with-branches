# $OpenBSD: Makefile,v 1.10 2002/05/11 00:20:19 espie Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kadmind
MAN=	${PROG}.8

SRCS=	kadmind.c server.c kadm_conn.c

.PATH:	${KRB5SRC}/kadmin
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
DPADD+=	${LIBHDB} ${LIBKADM5SRV} ${LIBKRB5} ${LIBDES} ${LIBASN1} \
	${LIBCRYPTO} ${LIBCOM_ERR} ${LIBUTIL}
LDADD+=	-lhdb -lkadm5srv -lkrb5 -ldes ${LIBROKEN} -lasn1 \
	-lcrypto -lcom_err -lutil

.include <bsd.prog.mk>
