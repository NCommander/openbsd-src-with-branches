#	$OpenBSD: Makefile,v 1.2 1996/06/26 05:42:57 deraadt Exp $

MAN=	which.1

beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/which.csh ${DESTDIR}${BINDIR}/which

.include <bsd.prog.mk>
