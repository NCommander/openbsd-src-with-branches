#	$OpenBSD$
#

FILES= config.guess config.sub configure install-sh mkdirs.sh \
	move-if-change ltconfig ltmain.sh config.if

NOOBJ=	noobj

install:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} \
		-m ${BINMODE} ${FILES} ${DESTDIR}${GNUSYSTEM_AUX_DIR}

.include <bsd.prog.mk>
