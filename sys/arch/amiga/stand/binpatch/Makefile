#	$OpenBSD: Makefile,v 1.4 1999/07/09 18:48:04 deraadt Exp $
#	$NetBSD: Makefile,v 1.6 1996/09/27 03:23:12 chopps Exp $

.if ${MACHINE} == "amiga"
PROG=binpatch
BINDIR=/sbin
LDFLAGS+=-static
.else
NOPROG=Yes
.endif

MAN=binpatch.8
MANSUBDIR=/amiga

.include <bsd.prog.mk>
