#	$OpenBSD: Makefile,v 1.5 2000/04/01 08:05:05 deraadt Exp $

NOPROG=
NOMAN=

regress: cfman no3man scatman
	perl ${.CURDIR}/cfman /usr/share/man
	perl ${.CURDIR}/no3man /usr/share/man
	perl ${.CURDIR}/scatman /usr/share/man

install:

.include <bsd.prog.mk>

