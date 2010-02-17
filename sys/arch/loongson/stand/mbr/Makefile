#	$OpenBSD: Makefile,v 1.1 2001/09/01 15:35:00 drahn Exp $

.include	"${.CURDIR}/../Makefile.inc"

CLEANFILES= mbr

all: mbr

mbr: mbr.uu
	uudecode ${.CURDIR}/mbr.uu

realinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 mbr \
		${DESTDIR}/usr/mdec


.include <bsd.prog.mk>
