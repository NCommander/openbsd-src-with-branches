#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:44:35 deraadt Exp $

PROG=	vnconfig
MAN=	vnconfig.8
DPADD=	${LIBUTIL}
LDADD=	-lutil -lcrypto
SRCS=vnconfig.c pkcs5_pbkdf2.c

.include <bsd.prog.mk>
