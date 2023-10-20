#	$OpenBSD: Makefile,v 1.2 2014/12/23 19:32:16 pascal Exp $

PROG=		bgplgd
SRCS=		bgplgd.c slowcgi.c qs.c
CFLAGS+=	-Wall
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CLFAGS+=	-Wmissing-declarations -Wredundant-decls
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare
LDADD=  -levent
DPADD=  ${LIBEVENT}
MAN=		bgplgd.8

.include <bsd.prog.mk>
