#	$OpenBSD: Makefile,v 1.3 1996/06/26 05:32:24 deraadt Exp $

NOMAN=

beforeinstall:
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/cpp.sh ${DESTDIR}${BINDIR}/cpp

.include <bsd.prog.mk>
