#	$OpenBSD$
#	@(#)Makefile	8.1 (Berkeley) 7/24/94

PROG=	gomoku
SRCS=	bdinit.c bdisp.c main.c makemove.c pickmove.c stoc.c
MAN=	gomoku.6
DPADD=	${LIBOCURSES} ${LIBTERMCAP}
LDADD=	-locurses -ltermcap
HIDEGAME=hidegame

.include <bsd.prog.mk>
