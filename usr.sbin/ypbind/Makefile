#	$OpenBSD: Makefile,v 1.4 1997/09/21 11:44:36 deraadt Exp $

PROG=	ypbind
MAN=	ypbind.8
CFLAGS+=-DDAEMON -DHEURISTIC

LDADD=-lrpcsvc
DPADD=${LIBRPCSVC}

.include <bsd.prog.mk>
