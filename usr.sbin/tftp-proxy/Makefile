#	$OpenBSD: Makefile,v 1.3 2012/07/11 12:07:32 dlg Exp $

PROG=	tftp-proxy
SRCS=	tftp-proxy.c filter.c
MAN=	tftp-proxy.8
LDADD=	-levent
DPADD=	${LIBEVENT}

CFLAGS+=-Wall -Werror
CDIAGFLAGS=

.include <bsd.prog.mk>
