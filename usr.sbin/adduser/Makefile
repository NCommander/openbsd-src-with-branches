#	$OpenBSD$

MAN=   adduser.8 adduser_proc.8

beforeinstall:
	install ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/adduser.perl ${DESTDIR}${BINDIR}/adduser

.include <bsd.prog.mk>
