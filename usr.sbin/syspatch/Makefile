#	$OpenBSD$

MAN=	syspatch.8

SCRIPT=	syspatch.sh

realinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/${SCRIPT} ${DESTDIR}${BINDIR}/syspatch

.include <bsd.prog.mk>
