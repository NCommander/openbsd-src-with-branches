#	$OpenBSD: Makefile,v 1.3 1997/04/27 20:55:41 millert Exp $

PROG=	caesar
MAN=	caesar.6
DPADD=	${LIBM}
LDADD=	-lm
MLINKS=	caesar.6 rot13.6

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/rot13.sh ${DESTDIR}/usr/games/rot13

.include <bsd.prog.mk>
