# $OpenBSD: Makefile,v 1.7 2003/05/17 01:31:21 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	ipropd-slave
MAN=	${PROG}.8
NOMAN=

SRCS=	ipropd_slave.c
SRCS+=	${PRINTVER}

.PATH:	${KRB5SRC}/lib/kadm5

CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5

DPADD+= ${LIBKADM5SRV} ${LIBKRB5} ${LIBCRYPTO} ${LIBUTIL} ${LIBDES}
LDADD+=	-lkadm5srv -lkrb5 -lcrypto -lutil -ldes

.include <bsd.prog.mk>
