# $OpenBSD$

PROG	= kstring2key

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/appl/afsutil

CFLAGS	+= -I.
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes

SRCS	= kstring2key.c

NOMAN	=

.include <bsd.prog.mk>
