# $OpenBSD: Makefile,v 1.1 2016/07/19 16:54:26 reyk Exp $

.PATH:		${.CURDIR}/../switchd

PROG=		switchctl
MAN=		switchctl.8
SRCS=		log.c switchctl.c parser.c util.c

LDADD=		-lutil
DPADD=		${LIBUTIL}

CFLAGS+=	-Wall -I${.CURDIR} -I${.CURDIR}/../switchd
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=	-Wmissing-declarations
CFLAGS+=	-Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=	-Wsign-compare

.include <bsd.prog.mk>
