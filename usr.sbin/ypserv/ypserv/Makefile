#	$OpenBSD$

PROG=	ypserv
SRCS=	ypserv.c ypserv_proc.c ypserv_db.c acl.c yplog.c ypdb.c ypserv_xdr.c
MAN=	ypserv.acl.5 securenet.5 ypserv.8
.PATH:	${.CURDIR}/../common
CFLAGS+=-DDAEMON -I${.CURDIR}/../common
#CFLAGS=-DDAEMON -I${.CURDIR}/../common  -DDEBUG -g

.include <bsd.prog.mk>
