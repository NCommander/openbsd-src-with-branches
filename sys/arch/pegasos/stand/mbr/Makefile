#	$OpenBSD$

CLEANFILES= mbr

all: mbr

mbr:
	dd if=/dev/zero of=mbr bs=512 count=1

realinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 mbr ${DESTDIR}/usr/mdec


.include <bsd.prog.mk>
