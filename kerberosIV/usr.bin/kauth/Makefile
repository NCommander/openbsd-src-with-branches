# $OpenBSD$

PROG	= kauth

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/appl/kauth

CFLAGS	+= -I.
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes

SRCS	= kauth.c rkinit.c encdata.c marshall.c

.include <bsd.prog.mk>
