#	$OpenBSD: Makefile,v 1.2 2001/01/28 19:34:35 niklas Exp $

PROG=	tftpd
SRCS=	tftpd.c
LDADD=	-levent
DPADD=	${LIBEVENT}
CFLAGS+= -Wall -Werror

MAN=	tftpd.8

.include <bsd.prog.mk>
