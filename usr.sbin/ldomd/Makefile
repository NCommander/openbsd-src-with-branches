#	$OpenBSD: Makefile,v 1.4 2012/10/21 12:56:45 kettenis Exp $

.PATH:	${.CURDIR}/../ldomctl

PROG=	ldomd
SRCS=	ldomd.c ds.c mdesc.c util.c
NOMAN=
CFLAGS+= -Wall
CFLAGS+= -I${.CURDIR}/../ldomctl
DEBUG= -g

.include <bsd.prog.mk>
