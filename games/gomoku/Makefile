#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:36:00 deraadt Exp $

PROG=	gomoku
SRCS=	bdinit.c bdisp.c main.c makemove.c pickmove.c stoc.c
MAN=	gomoku.6
DPADD=	${LIBCURSES}
LDADD=	-lcurses
HIDEGAME=hidegame

.include <bsd.prog.mk>
