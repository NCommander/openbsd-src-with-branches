# $OpenBSD: Makefile,v 1.4 2001/07/01 21:47:17 deraadt Exp $

PROG	= kdb_init

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/admin

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkafs -lkdb -lkrb -lcrypto

SRCS	= kdb_init.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
