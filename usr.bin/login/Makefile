#	$OpenBSD: Makefile,v 1.16 2001/05/25 12:17:55 hin Exp $

PROG=	login
SRCS=	login.c failedlogin.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
CFLAGS+= -Wall -Wno-unused

BINOWN=	root
BINMODE=4555

.include <bsd.prog.mk>
