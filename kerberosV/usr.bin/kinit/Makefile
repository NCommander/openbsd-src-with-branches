# $OpenBSD: Makefile,v 1.1 2001/05/25 08:20:34 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kinit

SRCS=	kinit.c kinit_options.c

.PATH:	${KRB5SRC}/kuser

LDADD+=	-lkrb5 -ldes -lkafs -l45 -lkrb -lkafs ${LIBROKEN} -lasn1 -lcrypto

.include <bsd.prog.mk>