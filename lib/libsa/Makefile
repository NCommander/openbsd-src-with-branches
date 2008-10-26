#	$OpenBSD: Makefile,v 1.1.2.4 2008/09/16 11:03:52 alex Exp $

LIB=	sa
MAN=	libsa.3
SRCS=	aucat.c sun.c libsa.c
CFLAGS+=-Wall -Wstrict-prototypes -Werror -Wundef -DDEBUG \
	-I${.CURDIR} -I${.CURDIR}/../../usr.bin/aucat

includes:
	@cd ${.CURDIR}; cmp -s libsa.h ${DESTDIR}/usr/include/libsa.h || \
		${INSTALL} ${INSTALL_COPY} -o $(BINOWN) -g $(BINGRP) \
		-m 444 libsa.h ${DESTDIR}/usr/include

.include <bsd.lib.mk>
