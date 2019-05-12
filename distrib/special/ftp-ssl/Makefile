#	$OpenBSD: Makefile,v 1.1 2016/12/21 20:55:42 deraadt Exp $

CFLAGS+=-DSMALL

PROG=	ftp
SRCS=	file.c ftp.c http.c main.c progressmeter.c url.c util.c xmalloc.c
.PATH:	${.CURDIR}/../../../usr.bin/ftp

LDADD+=	-lutil -ltls -lssl -lcrypto
DPADD+=	${LIBUTIL} ${LIBTLS} ${LIBSSL} ${LIBCRYPTO}

.include <bsd.prog.mk>
