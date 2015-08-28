#	$OpenBSD: Makefile,v 1.6 2013/04/16 16:51:35 deraadt Exp $

PROG=	ping

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual

MAN=	ping.8
BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
