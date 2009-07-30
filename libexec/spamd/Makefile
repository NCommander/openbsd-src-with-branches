#	$OpenBSD: Makefile,v 1.8 2005/05/24 22:23:04 millert Exp $

PROG=	spamd
SRCS=	spamd.c sdl.c grey.c sync.c
MAN=	spamd.8

CFLAGS+= -Wall -Wstrict-prototypes

LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}

.include <bsd.prog.mk>
