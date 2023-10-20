#	$OpenBSD: Makefile,v 1.2 2023/07/23 23:21:19 kn Exp $

PROG=	mount
SRCS=	mount.c
COPTS+=	-DSMALL
DPADD=	${LIBUTIL}
LDADD=	-lutil

.PATH:	${.CURDIR}/../../../sbin/mount
.include <bsd.prog.mk>
