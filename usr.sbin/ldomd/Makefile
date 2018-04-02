#	$OpenBSD: Makefile,v 1.2 2012/10/27 18:34:03 kettenis Exp $

.if ${MACHINE} == "sparc64"

.PATH:	${.CURDIR}/../ldomctl

PROG=	ldomd
SRCS=	ldomd.c ds.c mdesc.c util.c var-config.c
CFLAGS+=-Wall
CFLAGS+=-I${.CURDIR}/../ldomctl
DEBUG=	-g

.else

NOPROG=	yes

.endif

MAN=	ldomd.8
MANSUBDIR=sparc64

.include <bsd.prog.mk>
