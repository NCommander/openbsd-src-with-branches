#	$OpenBSD: Makefile,v 1.5 2013/04/15 15:51:46 deraadt Exp $

PROG=	traceroute

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual

MAN=	traceroute.8
BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
