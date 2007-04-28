#	$OpenBSD: Makefile,v 1.2 1996/04/22 01:26:37 deraadt Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/5/93

FILES=	README
SUBDIR=	psd smm usd

all depend includes lint tags:

realinstall:
	install -c -o ${DOCOWN} -g ${DOCGRP} -m ${DOCMODE} ${FILES} \
	    ${DESTDIR}${DOCDIR}

.include <bsd.subdir.mk>
