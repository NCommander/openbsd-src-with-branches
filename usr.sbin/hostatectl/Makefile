#	$OpenBSD$

.PATH:		${.CURDIR}/../hostated

PROG=		hostatectl
SRCS=		buffer.c imsg.c log.c hostatectl.c parser.c

MAN=		hostatectl.8

CFLAGS+=	-Wall -Werror -I${.CURDIR} -I${.CURDIR}/../hostated
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded

.include <bsd.prog.mk>
