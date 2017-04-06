#	$OpenBSD: Makefile,v 1.11 2013/11/21 08:15:46 ratchov Exp $

LIB=	sndio
MAN=	sio_open.3 mio_open.3 sndio.7
SRCS=	debug.c aucat.c sio_aucat.c sio_sun.c sio.c \
	mio_rmidi.c mio_aucat.c mio.c
CFLAGS+=-DDEBUG
COPTS+=	-Wall -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wundef

.include <bsd.lib.mk>
