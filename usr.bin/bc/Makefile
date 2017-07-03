#	$OpenBSD: Makefile,v 1.8 2015/10/10 19:28:54 deraadt Exp $

PROG=		bc
SRCS=		bc.y scan.l tty.c dc.c bcode.c inout.c mem.c stack.c
CPPFLAGS+=	-I. -I${.CURDIR}
CFLAGS+=	-Wall -Wno-unused
YFLAGS+=
LDADD+=		-ledit -lcurses -lcrypto
DPADD+=		${LIBEDIT} ${LIBCURSES} ${LIBCRYPTO}

.PATH:		${.CURDIR}/../dc

bc.c y.tab.h: bc.y
	${YACC} -d ${.CURDIR}/bc.y
	mv y.tab.c bc.c

scan.o: y.tab.h

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/bc.library \
	    ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
