# $OpenBSD$

PROG	= kprop

KRBDEPTH=../..
.include <../../Makefile.inc>

.PATH:	${KRBSRC}/slave ${KRBSRC}/lib/sl

CFLAGS	+= -I. -I${KRBSRC}/lib/kadm -I${KRBSRC}/lib/krb -I${KRBSRC}/lib/kdb
LDADD	+= ${LIBROKEN} -lkrb -lkafs -ldes -lkadm -lcom_err -lkdb -lacl

SRCS	= kprop.c

MAN	= ${PROG}.8

.include <bsd.prog.mk>
