#	$OpenBSD: Makefile,v 1.7 1999/12/23 07:14:12 mickey Exp $

.include <bsd.own.mk>

NOMAN=

PROG=cpp
SRCS=
INSTALL_STRIP=

cpp: cpp.sh
.if ${USE_GCC3:L} == "no"
	sed -e 's/@dollaropt@/-$$/' ${.CURDIR}/cpp.sh >$@
.else
	sed -e 's/@dollaropt@//' ${.CURDIR}/cpp.sh >$@
.endif

.include <bsd.prog.mk>
