#	$OpenBSD: Makefile,v 1.35 2001/06/28 02:32:32 hin Exp $

.PATH:		${.CURDIR}/..

CARDLET=	Ssh.bin
DATADIR=	/usr/libdata/ssh

all: ${CARDLET}

clean:
	rm -f ${CARDLET}

install: ${CARDLET}
	echo "installing ${CARDLET} into ${DESTDIR}${DATADIR}"; \
	install -c -m ${LIBMODE} -o ${LIBOWN} -g ${LIBGRP} \
	    ${CARDLET} ${DESTDIR}${DATADIR}

Ssh.bin: ${.CURDIR}/Ssh.bin.uu
	uudecode ${.CURDIR}/$@.uu

.include <bsd.prog.mk>
