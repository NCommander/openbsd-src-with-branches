#	$OpenBSD: Makefile,v 1.1 2018/07/10 22:12:43 florian Exp $

PROG=	unwindctl
SRCS=	unwindctl.c parser.c

MAN=	unwindctl.8

CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../../sbin/unwind
LDADD=	-lutil
DPADD=	${LIBUTIL}

.include <bsd.prog.mk>
