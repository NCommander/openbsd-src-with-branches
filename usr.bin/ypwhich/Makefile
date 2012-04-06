#	$OpenBSD: Makefile,v 1.5 1999/03/19 22:21:27 deraadt Exp $
#	from: @(#)Makefile	5.8 (Berkeley) 7/28/90

PROG=	ypwhich
SRCS=	ypwhich.c yplib_host.c
MAN=	ypwhich.1

.PATH:	${.CURDIR}/../../usr.sbin/ypserv/common
CFLAGS+=-I${.CURDIR}/../../usr.sbin/ypserv/common

.include <bsd.prog.mk>
