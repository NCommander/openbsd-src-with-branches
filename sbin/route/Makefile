#	$OpenBSD: Makefile,v 1.10 2005/11/27 18:27:41 deraadt Exp $

PROG=	route
MAN=	route.8
SRCS=	route.c show.c

route.o .depend lint tags: keywords.h

keywords.h: keywords.sh
	/bin/sh ${.CURDIR}/keywords.sh

.include <bsd.prog.mk>
