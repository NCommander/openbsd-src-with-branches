#	$OpenBSD: Makefile,v 1.3 2012/09/02 23:07:26 deraadt Exp $

CLEANFILES= mbr

all: mbr

mbr: mbr.uu
	uudecode ${.CURDIR}/mbr.uu

realinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 mbr ${DESTDIR}/usr/mdec


.include <bsd.prog.mk>
