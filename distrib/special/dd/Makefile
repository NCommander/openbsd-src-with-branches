#	$OpenBSD: Makefile,v 1.2 2008/09/15 18:13:36 kurt Exp $

# Define NO_CONV to disable character conversion
CFLAGS+=-DNO_CONV

PROG=	dd
SRCS=	args.c conv.c dd.c misc.c position.c

.PATH:	${.CURDIR}/../../../bin/dd

.include <bsd.prog.mk>
