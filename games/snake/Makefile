#	$OpenBSD: Makefile,v 1.10 2015/11/24 03:10:10 deraadt Exp $
#	@(#)Makefile	8.1 (Berkeley) 5/31/93

PROG=	snake
SRCS=	snake.c snscore.c
MAN=	snake.6
DPADD=	${LIBM} ${LIBCURSES}
LDADD=	-lm -lcurses

.include <bsd.prog.mk>
