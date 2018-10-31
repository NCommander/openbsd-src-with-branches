#	$OpenBSD: Makefile,v 1.11 2016/05/04 19:48:08 jca Exp $

CFLAGS+=-DSMALL -DNOSSL

PROG=	ftp
SRCS=	fetch.c ftp.c main.c small.c util.c
.PATH:	${.CURDIR}/../../../usr.bin/ftp

LDADD+=	-lutil
DPADD+=	${LIBUTIL}

.include <bsd.prog.mk>
