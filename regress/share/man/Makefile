#	$OpenBSD: Makefile,v 1.6 2000/04/02 18:47:50 deraadt Exp $

REGRESSTARGETS=do-cfman do-no3man do-scatman
do-cfman:
	perl ${.CURDIR}/cfman /usr/share/man
do-no3man:
	perl ${.CURDIR}/no3man /usr/share/man
do-scatman:
	perl ${.CURDIR}/scatman /usr/share/man

.include <bsd.regress.mk>

