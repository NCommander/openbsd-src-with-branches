#	$OpenBSD: Makefile,v 1.8 2003/07/29 18:36:30 jmc Exp $

COPTS+=-Os
PROG=	ed
CFLAGS+=-DBACKWARDS
SRCS=	 buf.c glbl.c io.c main.c re.c sub.c undo.c
LDSTATIC=-static
MAN=

.PATH:	${.CURDIR}/../../../bin/ed
.include <bsd.prog.mk>
