#	$OpenBSD$

PROG=	tetris
SRCS=	input.c screen.c shapes.c scores.c tetris.c
MAN=	tetris.6
DPADD=	${LIBTERMCAP}
LDADD=	-ltermcap
HIDEGAME=hidegame

.include <bsd.prog.mk>
