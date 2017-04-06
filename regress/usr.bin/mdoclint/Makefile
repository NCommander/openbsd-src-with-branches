#	$OpenBSD$

MAN=	mdoclint.1

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/mdoclint ${DESTDIR}/usr/bin/mdoclint

.include <bsd.prog.mk>
