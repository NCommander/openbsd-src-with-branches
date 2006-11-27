# $OpenBSD: Makefile,v 1.2 2006/11/27 17:01:56 ckuethe Exp $

NOMAN=chris is a lazy slacker

beforeinstall: 
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/pkg-config ${DESTDIR}${BINDIR}/pkg-config

.include <bsd.prog.mk>
