# $OpenBSD: Makefile,v 1.1 2001/05/25 08:21:13 hin Exp $

KRBDEPTH=../..
.include <${.CURDIR}/../../Makefile.inc>

PROG=	ktutil
MAN=	ktutil.8

SRCS=	add.c change.c copy.c get.c ktutil.c list.c purge.c remove.c

.PATH:	${KRB5SRC}/admin
CFLAGS+=-I${KRB5SRC}/lib/krb5 -I${KRB5SRC}/lib/kadm5
LDADD+=	${LIBSL} -lreadline -lcurses -lkrb -ldes -lhdb \
	-lkadm5srv -lkadm5clnt -lhdb -lkrb5 ${LIBROKEN} -lasn1 -lcrypto

.include <bsd.prog.mk>
