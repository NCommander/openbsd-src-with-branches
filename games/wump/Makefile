#	$OpenBSD: Makefile,v 1.3 1997/04/27 20:55:49 millert Exp $

PROG=	wump
MAN=	wump.6
HIDEGAME=hidegame

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${.CURDIR}/wump.info ${DESTDIR}/usr/share/games

.include <bsd.prog.mk>
