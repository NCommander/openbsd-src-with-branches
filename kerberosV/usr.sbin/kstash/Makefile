# $OpenBSD: Makefile,v 1.5 2003/07/14 16:14:45 mho Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kstash
NOMAN=

SRCS=	kstash.c \
	sl.c
SRCS+=	${PRINTVER}

.PATH:	${KRB5SRC}/kdc
.PATH:	${KRB5SRC}/lib/sl

CFLAGS+=-I${KRB5SRC}/lib/krb5

DPADD+= ${LIBREADLINE} ${LIBCURSES} ${LIBDES} ${LIBKRB5} \
	${LIBCRYPTO}
LDADD+=	-lreadline -lcurses -ldes -lkrb5 \
	-lcrypto

.include <bsd.prog.mk>
