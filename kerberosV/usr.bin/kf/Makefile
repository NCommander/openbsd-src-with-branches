# $OpenBSD: Makefile,v 1.2 2002/05/11 00:20:19 espie Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kf

SRCS=	kf.c

.PATH:	${KRB5SRC}/appl/kf

DPADD+= ${LIBKRB5} ${LIBDES} ${LIBCRYPTO}
LDADD+=	-lkrb5 -ldes -lcrypto

.include <bsd.prog.mk>
