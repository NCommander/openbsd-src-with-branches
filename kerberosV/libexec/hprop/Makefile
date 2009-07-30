# $OpenBSD: Makefile,v 1.6 2003/05/17 01:31:21 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	hprop
MAN=	${PROG}.8

SRCS=	hprop.c mit_dump.c v4_dump.c hprop.h kadb.h
SRCS+=	${PRINTVER}
SRCS+=	tm2time.c

.PATH:	${KRB5SRC}/kdc
.PATH:	${ROKENSRC}

CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5

DPADD+= ${LIBKADM5SRV} ${LIBDES} ${LIBCRYPTO}
LDADD+=	-lkadm5srv -lkrb5 -ldes -lcrypto

.include <bsd.prog.mk>
