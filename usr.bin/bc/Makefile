#	$OpenBSD$

PROG=		bc
SRCS=		bc.c scan.c
CPPFLAGS+=	-I. -I${.CURDIR}
CFLAGS+=	-Wall -Wno-unused
YFLAGS+=
CLEANFILES+=	bc.c y.tab.h scan.c lex.yy.c

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/bc.library \
	    ${DESTDIR}/usr/share/misc

.include <bsd.prog.mk>
