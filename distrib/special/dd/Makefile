#	$OpenBSD: Makefile,v 1.4 2002/11/09 20:18:28 krw Exp $

COPTS+=-Os
# Define NO_CONV to disable character conversion
CFLAGS+=-DNO_CONV
LDFLAGS=-static

PROG=	dd
SRCS=	args.c conv.c dd.c misc.c position.c
MAN=

.PATH:	${.CURDIR}/../../../bin/dd

.include <bsd.prog.mk>
