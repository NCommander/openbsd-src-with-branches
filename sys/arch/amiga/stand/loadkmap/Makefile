# $OpenBSD: Makefile,v 1.6 1999/07/09 18:48:05 deraadt Exp $
# $NetBSD: Makefile,v 1.5 1996/09/27 03:23:13 chopps Exp $

.if ${ARCH}=="amiga"
PROG=loadkmap
BINDIR=/usr/sbin
SUBDIR=din es fr sw us
.else
NOPROG=Yes
.endif

MAN=loadkmap.8
MANSUBDIR=/amiga
.include <bsd.prog.mk>
