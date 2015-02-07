#	$OpenBSD: Makefile,v 1.9 2007/03/04 03:19:41 beck Exp $

PROG=	spamd
SRCS=	spamd.c sdl.c gdcopy.c grey.c sync.c
MAN=	spamd.8

CFLAGS+= -Wall -Wstrict-prototypes

LDADD+=	-ltls -lssl -lcrypto
DPADD+=	${LIBTLS} ${LIBSSL} ${LIBCRYPTO}

.include <bsd.prog.mk>
