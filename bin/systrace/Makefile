#	$OpenBSD: Makefile,v 1.5 1999/02/23 23:54:16 art Exp $

PROG=	systrace
CFLAGS+= -I.
SRCS=	filter.c intercept-translate.c intercept.c \
	openbsd-syscalls.c util.c \
	policy.c systrace-errno.h systrace-error.c \
	systrace-translate.c systrace.c \
	parse.c lex.l
YFLAGS=	-d
CLEANFILES+= parse.c parse.h
NOMAN=

parse.c: parse.y
	${YACC} -d -o parse.c ${.CURDIR}/parse.y

.include <bsd.prog.mk>
