#	$OpenBSD: Makefile,v 1.1 2013/05/23 12:03:24 florian Exp $

PROG=	slowcgi
SRCS=	slowcgi.c
CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
LDADD=	-levent
DPADD=	${LIBEVENT}
MAN=	slowcgi.8
.include <bsd.prog.mk>
