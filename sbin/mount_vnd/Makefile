# $OpenBSD$

MAN=mount_vnd.8

beforeinstall:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/mount_vnd ${DESTDIR}${BINDIR}/mount_vnd

.include <bsd.prog.mk>
