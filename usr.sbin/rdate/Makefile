#	$OpenBSD: Makefile,v 1.1 2002/05/15 08:26:30 jakob Exp $

PROG=	rdate
SRCS=	rdate.c rfc868time.c ntp.c
DPADD+= ${LIBUTIL}
LDADD+= -lutil

MAN=	rdate.8

.include <bsd.prog.mk>
