# $OpenBSD: Makefile,v 1.3 2006/06/16 22:27:53 niallo Exp $

NOMAN=

FIRM=	zd1211

PROG=	build

CLEANFILES+= ${FIRM} ${PROG}

all: ${FIRM}

${FIRM}: build
	${.OBJDIR}/build

realinstall:

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${FIRM} ${DESTDIR}/etc/firmware
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 \
	    ${.CURDIR}/zd1211-licence ${DESTDIR}/etc/firmware


.include <bsd.prog.mk>
