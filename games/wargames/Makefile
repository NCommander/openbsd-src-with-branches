#	$OpenBSD: Makefile,v 1.3 1997/04/27 20:55:48 millert Exp $

NOOBJ=	noobj
all wargames clean cleandir depend lint tags:

install:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/wargames.sh ${DESTDIR}/usr/games/wargames

.include <bsd.prog.mk>
