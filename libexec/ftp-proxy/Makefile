#	$OpenBSD$
#	@(#)Makefile	8.2 (Berkeley) 4/4/94

PROG=	ftp-proxy
CFLAGS+=-Wall -Werror
SRCS=	ftp-proxy.c getline.c util.c 
MAN=	ftp-proxy.8

.include <bsd.own.mk>

LDADD+=	-lwrap
DPADD+=	${LIBWRAP}

.include <bsd.prog.mk>
