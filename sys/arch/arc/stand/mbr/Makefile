#	$OpenBSD$

NOOBJ=	noobj

CLEANFILES= mbr msdos5mb.gz

all: mbr msdos5mb.gz

mbr: mbr.uu
	uudecode mbr.uu

msdos5mb.gz: msdos5mb.gz.uu
	uudecode msdos5mb.gz.uu

realinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 mbr msdos5mb.gz \
		${DESTDIR}/usr/mdec


.include <bsd.prog.mk>
