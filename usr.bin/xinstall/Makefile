#	$OpenBSD: Makefile,v 1.5 1997/09/21 11:52:11 deraadt Exp $

PROG=	xinstall
SRCS=	xinstall.c
MAN=	install.1

realinstall:
	${INSTALL} ${INSTALL_COPY} ${INSTALL_STRIP} -o ${BINOWN} -g ${BINGRP} \
	    -m ${BINMODE} ${PROG} ${DESTDIR}${BINDIR}/install

.include <bsd.prog.mk>
