# $OpenBSD: Makefile,v 1.3 2001/01/09 02:41:21 angelos Exp $

PROG	= kdb_init

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/admin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/kdb
LDADD	+= -lkafs -lkrb5 -lkrb -lkdb -ldes -lcrypto -lasn1 ${LIBROKEN}

SRCS	= kdb_init.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
