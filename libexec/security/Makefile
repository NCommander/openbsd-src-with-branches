#	$OpenBSD$

realinstall: 
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/security ${DESTDIR}${BINDIR}/security

.include <bsd.prog.mk>
