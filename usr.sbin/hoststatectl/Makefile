#	$OpenBSD: Makefile,v 1.1 2006/12/16 11:45:07 reyk Exp $

.PATH:		${.CURDIR}/../hoststated

PROG=		hoststatectl
SRCS=		buffer.c imsg.c log.c hoststatectl.c parser.c

MAN=		hoststatectl.8

CFLAGS+=	-Wall -Werror -I${.CURDIR} -I${.CURDIR}/../hoststated
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare -Wbounded

.include <bsd.prog.mk>
