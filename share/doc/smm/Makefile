#	$OpenBSD: Makefile,v 1.8 2010/01/04 17:50:39 deraadt Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/10/93

DOCDIR=	/usr/share/doc/smm
FILES=	Makefile

beforeinstall:
	install -c -o ${DOCOWN} -g ${DOCGRP} -m ${DOCMODE} ${FILES} \
	    ${DESTDIR}${DOCDIR}

.include <bsd.subdir.mk>
