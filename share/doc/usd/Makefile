#	$OpenBSD: Makefile,v 1.9 2010/01/04 17:50:39 deraadt Exp $

DOCDIR=	/usr/share/doc/usd
FILES=	Makefile

beforeinstall:
	install -c -o ${DOCOWN} -g ${DOCGRP} -m ${DOCMODE} ${FILES} \
	    ${DESTDIR}${DOCDIR}

.include <bsd.subdir.mk>
