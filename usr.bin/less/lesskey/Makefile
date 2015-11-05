#	$OpenBSD: Makefile,v 1.1 2011/09/16 18:12:09 shadchin Exp $

.PATH:	${.CURDIR}/..

PROG=	lesskey
SRCS=	lesskey.c version.c

MAN=	lesskey.1
BINDIR=	/usr/bin

CFLAGS+=-I${.CURDIR}/..

CDIAGFLAGS+=-Wall -W

.include <bsd.prog.mk>
