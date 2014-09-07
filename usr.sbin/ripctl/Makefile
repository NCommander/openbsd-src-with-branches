#	$OpenBSD: Makefile,v 1.1 2006/10/18 16:15:25 norby Exp $

.PATH:		${.CURDIR}/../ripd

PROG=	ripctl
SRCS=	log.c ripctl.c parser.c
CFLAGS+= -Wall
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+= -Wsign-compare
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../ripd
LDADD=	-lutil
DPADD=	${LIBUTIL}
MAN=	ripctl.8

.include <bsd.prog.mk>
