# $OpenBSD$

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	klist

SRCS=	klist.c

.PATH:	${KRB5SRC}/kuser

CFLAGS+=-I${KRB5SRC}/lib/roken
LDADD+=	-lkrb5 -ldes -lkafs -lkrb ${LIBROKEN} 

.include <bsd.prog.mk>