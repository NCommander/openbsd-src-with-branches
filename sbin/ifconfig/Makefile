#	$OpenBSD: Makefile,v 1.14 2016/05/03 17:52:33 jca Exp $

PROG=	ifconfig
SRCS=	ifconfig.c brconfig.c sff.c
MAN=	ifconfig.8

LDADD=	-lutil -lm
DPADD=	${LIBUTIL}

.include <bsd.prog.mk>
