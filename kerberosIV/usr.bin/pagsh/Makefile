# $OpenBSD$

PROG	= pagsh

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/appl/afsutil

CFLAGS	+= -I.
LDADD	+= ${LIBROKEN} -lkrb -lkafs

SRCS	= pagsh.c

.include <bsd.prog.mk>
