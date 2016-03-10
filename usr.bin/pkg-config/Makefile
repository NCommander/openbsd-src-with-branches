# $OpenBSD: Makefile,v 1.4 2006/11/28 00:48:08 ckuethe Exp $

MAN=pkg-config.1

LIBDIR = /usr/libdata/perl5/OpenBSD

realinstall: 
	${INSTALL} -d -o ${LIBOWN} -g ${LIBGRP} -m ${DIRMODE} \
		${DESTDIR}${LIBDIR}
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
		${.CURDIR}/OpenBSD/PkgConfig.pm ${DESTDIR}${LIBDIR}
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/pkg-config ${DESTDIR}${BINDIR}/pkg-config

NOPROG = 
.include <bsd.prog.mk>
