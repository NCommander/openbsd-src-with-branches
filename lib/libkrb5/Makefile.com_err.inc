# $OpenBSD: Makefile.kafs.inc,v 1.4 2003/05/17 20:07:23 mho Exp $

CFLAGS += -I${KRB5SRC}/lib/com_err

.PATH:	${KRB5SRC}/lib/com_err

SRCS+=	com_err.c error.c

foo:
	echo ${KRB5SRC}/lib/com_err

