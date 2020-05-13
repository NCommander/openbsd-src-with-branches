#	$OpenBSD$

SCRIPT=	reorder_kernel.sh

realinstall:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/${SCRIPT} ${DESTDIR}${BINDIR}/reorder_kernel

.include <bsd.prog.mk>
