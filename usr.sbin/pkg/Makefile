#	$OpenBSD$

NOMAN=
NOPROG=

afterinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/pkg.pl ${DESTDIR}${BINDIR}/pkg

.include <bsd.prog.mk>
