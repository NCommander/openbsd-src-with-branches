#	$OpenBSD: Makefile,v 1.1 2004/01/23 21:34:30 mcbride Exp $

PROG=	ifstated
SRCS=	ifstated.c parse.y
CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CLFAGS+= -Wmissing-declarations -Wredundant-decls
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
MAN=
LDADD+=-lutil -levent

.include <bsd.prog.mk>
