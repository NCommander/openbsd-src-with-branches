# $OpenBSD$

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	kinit

SRCS=	kinit.c kinit_options.c

.PATH:	${KRB5SRC}/kuser

LDADD+=	${LIBROKEN} -lkrb5 -ldes -lkafs -l45 -lkrb -lkafs5

.include <bsd.prog.mk>