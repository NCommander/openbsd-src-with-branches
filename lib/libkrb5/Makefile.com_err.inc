# $OpenBSD: Makefile.com_err.inc,v 1.1 2003/06/03 09:12:25 hin Exp $

DIR_COM_ERR=	${KRB5SRC}/lib/com_err
HDR_COM_ERR=	com_err.h com_right.h

.PATH:	${DIR_COM_ERR}

SRCS+=	com_err.c \
	error.c

CFLAGS+=	-I${DIR_COM_ERR}
