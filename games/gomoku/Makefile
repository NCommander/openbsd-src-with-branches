#	$OpenBSD: Makefile,v 1.2 1996/12/20 00:22:18 downsj Exp $

PROG=	gomoku
SRCS=	bdinit.c bdisp.c main.c makemove.c pickmove.c stoc.c
MAN=	gomoku.6
DPADD=	${LIBCURSES} ${LIBTERMLIB}
LDADD=	-lcurses -ltermlib
HIDEGAME=hidegame

.include <bsd.prog.mk>
