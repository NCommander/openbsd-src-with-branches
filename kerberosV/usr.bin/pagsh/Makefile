# $OpenBSD$

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	pagsh

.PATH:	${KRB5SRC}/appl/afsutil
NOMAN=

CFLAGS+=-I${KRB5SRC}/lib/roken
LDADD+=	${LIBROKEN} -lkrb5 -ldes -lkafs -lkrb

.include <bsd.prog.mk>