#	$OpenBSD: Makefile,v 1.14 2003/04/17 03:47:05 drahn Exp $
#	$NetBSD: Makefile,v 1.17 1995/10/24 22:18:34 jtc Exp $

MAN=	gzexe.1 zdiff.1 zforce.1 zmore.1 znew.1
MLINKS+= zdiff.1 zcmp.1
LINKS+=	${BINDIR}/zdiff ${BINDIR}/zcmp

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	   ${.CURDIR}/zforce ${.CURDIR}/gzexe ${.CURDIR}/znew \
	   ${.CURDIR}/zmore ${.CURDIR}/zdiff \
	   ${DESTDIR}${BINDIR}

.include <bsd.prog.mk>
