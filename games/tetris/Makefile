#	$OpenBSD: Makefile,v 1.7 2002/05/31 03:46:35 pjanzen Exp $

PROG=	tetris
SRCS=	input.c screen.c shapes.c scores.c tetris.c
MAN=	tetris.6
DPADD=	${LIBCURSES}
LDADD=	-lcurses

.include <bsd.prog.mk>
