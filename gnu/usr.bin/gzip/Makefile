#	$OpenBSD: Makefile,v 1.16 2003/06/23 19:11:54 deraadt Exp $
#	$NetBSD: Makefile,v 1.17 1995/10/24 22:18:34 jtc Exp $

MAN=	gzexe.1 zforce.1 znew.1

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	   ${.CURDIR}/zforce ${.CURDIR}/gzexe ${.CURDIR}/znew \
	   ${DESTDIR}${BINDIR}

.include <bsd.prog.mk>
