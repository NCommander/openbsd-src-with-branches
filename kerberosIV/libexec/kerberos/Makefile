# $OpenBSD$

PROG	= kerberos

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/server

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/acl -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes -lkdb

SRCS	= kerberos.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
