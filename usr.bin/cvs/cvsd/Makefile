#	$OpenBSD$

.PATH:	${.CURDIR}/..

PROG=	cvsd
SRCS=	cvsd.c buf.c conf.y log.c msg.c repo.c sock.c

BINDIR=	/usr/sbin
MAN=	cvsd.8 cvsd.conf.5

CFLAGS+= -Wall -I${.CURDIR}/..
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -DCVSD
YFLAGS=

.include <bsd.prog.mk>
