#	$OpenBSD: Makefile,v 1.8 2009/10/28 07:36:49 deraadt Exp $

CFLAGS+=-DSMALL -DINET6

PROG=	ftp
SRCS=	fetch.c ftp.c main.c small.c util.c
.PATH:	${.CURDIR}/../../../usr.bin/ftp

LDADD+=	-lcurses -lutil
DPADD+=	${LIBCURSES} ${LIBUTIL}

.include <bsd.prog.mk>
