#	$OpenBSD$

NOMAN=	noman

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/binstall.sh ${DESTDIR}${BINDIR}/binstall

.include <bsd.prog.mk>
