#	$OpenBSD: Makefile,v 1.3 1997/04/27 20:55:42 millert Exp $

PROG=	fish
MAN=	fish.6
HIDEGAME=hidegame

beforeinstall: 
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/fish.instr ${DESTDIR}/usr/share/games/fish.instr

.include <bsd.prog.mk>
