#	$OpenBSD: Makefile,v 1.4 1998/07/24 00:10:44 millert Exp $

PROG=	tetris
SRCS=	input.c screen.c shapes.c scores.c tetris.c
MAN=	tetris.6
DPADD=	${LIBCURSES}
LDADD=	-lcurses
HIDEGAME=hidegame

beforeinstall:
	@if [ ! -f ${DESTDIR}/var/games/tetris.scores ]; then \
	    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 664 \
		/dev/null ${DESTDIR}/var/games/tetris.scores ; \
	else \
		true ; \
	fi

.include <bsd.prog.mk>
