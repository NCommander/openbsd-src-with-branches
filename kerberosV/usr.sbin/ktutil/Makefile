# $OpenBSD$

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	ktutil
MAN=	ktutil.8

SRCS=	add.c change.c copy.c get.c ktutil.c list.c purge.c remove.c

.PATH:	${KRB5SRC}/admin
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
LDADD+=	${LIBSL} -lreadline -lcurses -lkrb -ldes -lhdb \
	-lkrb5 -lkadm5srv -lkadm5clnt ${LIBROKEN} -lasn1 -lhdb

.include <bsd.prog.mk>
