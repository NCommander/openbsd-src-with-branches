#	$OpenBSD: Makefile,v 1.8 2006/11/26 11:31:08 deraadt Exp $

PROG=	bioctl
SRCS=	bioctl.c pkcs5_pbkdf2.c
.PATH:	${.CURDIR}/../mount_vnd
CFLAGS+=-Wall -I${.CURDIR}/../mount_vnd
LDADD=	-lutil -lcrypto
DPADD=	${LIBUTIL}
MAN=	bioctl.8

.include <bsd.prog.mk>
