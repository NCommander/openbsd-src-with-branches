#	$OpenBSD$

MAN=	sendbug.1

beforeinstall:
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${.CURDIR}/sendbug.sh ${DESTDIR}${BINDIR}/sendbug

.include <bsd.prog.mk>
