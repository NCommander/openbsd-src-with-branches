# $OpenBSD$

PROG	= klist

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/kuser

CFLAGS	+= -I.
LDADD	+= ${LIBROKEN} -lkrb -lkafs

SRCS	= klist.c

.include <bsd.prog.mk>
