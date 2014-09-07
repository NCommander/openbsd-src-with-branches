#	$OpenBSD: Makefile,v 1.4 2012/03/08 03:17:10 dlg Exp $

PROG=	identd
SRCS=	identd.c
LDADD=	-levent
DPADD=	${LIBEVENT}
CFLAGS+= -Wall -Werror

MAN=	identd.8

.include <bsd.prog.mk>
