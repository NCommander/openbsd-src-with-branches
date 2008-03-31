# $OpenBSD: Makefile,v 1.3 2006/11/27 23:27:46 deraadt Exp $

MAN=pkg-config.1

beforeinstall: 
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/pkg-config ${DESTDIR}${BINDIR}/pkg-config

.include <bsd.prog.mk>
