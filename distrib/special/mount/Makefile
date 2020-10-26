#	$OpenBSD: Makefile,v 1.7 2014/07/14 06:39:23 jmc Exp $

PROG=	mount
SRCS=	mount.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	mount.8

.PATH:	${.CURDIR}/../../../sbin/mount
.include <bsd.prog.mk>
