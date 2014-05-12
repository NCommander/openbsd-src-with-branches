#	$OpenBSD: Makefile,v 1.5 2001/07/19 10:20:01 espie Exp $

PROG =	tsort
SRCS = tsort.c

CDIAGFLAGS = -Wall -Wno-char-subscripts -Wstrict-prototypes -pedantic -W
DPADD += ${LIBUTIL}
LDADD += -lutil

.include <bsd.prog.mk>
