# $OpenBSD$

SCRIPT=pkg-config

# Nothing to build or clean
all:
clean:

install: 
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${SCRIPT} ${DESTDIR}${BINDIR}/

.include <bsd.prog.mk>
