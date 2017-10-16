#	$OpenBSD: Makefile,v 1.2 2017/01/20 12:19:02 bluhm Exp $

XARGS=	/usr/bin/xargs

PROG= showargs

run-regress-showargs:
	sh ${.CURDIR}/xargs-L.sh ${XARGS}

.include <bsd.regress.mk>
