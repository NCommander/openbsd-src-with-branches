# $OpenBSD$

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kdestroy

SRCS=	kdestroy.c

.PATH:	${KRB5SRC}/kuser

LDADD+=	${LIBROKEN} -lkrb5 -ldes -lkafs -lkrb

.include <bsd.prog.mk>