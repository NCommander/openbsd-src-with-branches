# $OpenBSD: Makefile,v 1.3 2001/01/09 02:41:19 angelos Exp $

PROG	= afslog

KRBDEPTH=../..
.include "../../Makefile.inc"

.PATH:	${KRBSRC}/appl/afsutil

CFLAGS	+= -I.
LDADD	+= -lkafs -lkrb5 -lkrb -ldes -lcrypto -lasn1 ${LIBROKEN}

SRCS	= aklog.c

.include <bsd.prog.mk>
