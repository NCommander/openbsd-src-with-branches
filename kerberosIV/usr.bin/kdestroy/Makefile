# $OpenBSD$

PROG	= kdestroy

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/kuser

CFLAGS	+= -I.
LDADD	+= ${LIBROKEN} -lkrb -lkafs

SRCS	= kdestroy.c

.include <bsd.prog.mk>
