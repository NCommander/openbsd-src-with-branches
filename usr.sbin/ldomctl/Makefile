#	$OpenBSD: Makefile,v 1.4 2012/10/21 12:56:45 kettenis Exp $

.if ${MACHINE} == "sparc64"

PROG=	ldomctl
SRCS=	ldomctl.c ds.c mdesc.c util.c
CFLAGS+=-Wall
CFLAGS+=-I${.CURDIR}/../ldomd
DEBUG= -g

.else

NOPROG=	yes

.endif

MAN=	ldomctl.8
MANSUBDIR=sparc64

.include <bsd.prog.mk>
