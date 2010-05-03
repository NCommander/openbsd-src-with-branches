#	$OpenBSD: Makefile,v 1.8 2004/02/10 02:02:22 espie Exp $

.include <bsd.own.mk>

NOMAN=

PROG=cpp
SRCS=
INSTALL_STRIP=
USE_GCC3?="no"
USE_GCC4?="no"

cpp: cpp.sh
.if ${USE_GCC3:L} == "yes"
	sed -e 's/@GNUC@/-D__GNUC__/' -e 's/@dollaropt@//' ${.CURDIR}/cpp.sh >$@
.elif ${USE_GCC4:L} == "yes"
	sed -e 's/@GNUC@//' -e 's/@dollaropt@//' ${.CURDIR}/cpp.sh >$@
.else
	sed -e 's/@GNUC@/-D__GNUC__/' -e 's/@dollaropt@/-$$/' ${.CURDIR}/cpp.sh >$@
.endif

.include <bsd.prog.mk>
