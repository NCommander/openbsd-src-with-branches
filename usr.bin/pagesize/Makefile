#	$OpenBSD: Makefile,v 1.4 1997/04/27 20:56:52 millert Exp $

MAN=	pagesize.1

SCRIPT=	pagesize.sh

realinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/${SCRIPT} ${DESTDIR}${BINDIR}/pagesize

.include <bsd.prog.mk>
