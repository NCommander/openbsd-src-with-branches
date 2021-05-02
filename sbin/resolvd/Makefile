#	$OpenBSD$

PROG=	resolvd
SRCS=	resolvd.c

MAN=	resolvd.8

#DEBUG=	-g -DDEBUG=3 -O0

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare

.include <bsd.prog.mk>
