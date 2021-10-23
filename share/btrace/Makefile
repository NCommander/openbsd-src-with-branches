#	$OpenBSD$

FILES=	kprofile.bt runqlat.bt
NOOBJ=	noobj

install:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/btrace

.include <bsd.prog.mk>
