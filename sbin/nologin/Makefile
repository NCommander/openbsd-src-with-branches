#	$OpenBSD: Makefile,v 1.4 1995/03/18 14:59:08 cgd Exp $
#	$NetBSD: Makefile,v 1.4 1995/03/18 14:59:08 cgd Exp $

MAN=	nologin.8

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/nologin.sh ${DESTDIR}/sbin/nologin

.include <bsd.prog.mk>
