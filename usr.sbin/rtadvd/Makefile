#	$OpenBSD: Makefile,v 1.5 2006/12/19 14:50:29 itojun Exp $

PROG=	rtadvd
SRCS=	rtadvd.c rrenum.c advcap.c if.c config.c timer.c dump.c log.c

CFLAGS+=-Wall
LDADD+=	-lutil
DPADD+=	${LIBUTIL}
MAN=	rtadvd.8 rtadvd.conf.5

.include <bsd.prog.mk>
