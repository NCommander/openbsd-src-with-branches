#	$OpenBSD: Makefile,v 1.4 1997/09/17 14:19:04 deraadt Exp $

SUBDIR= bsd.rd floppy floppyB # floppyC

unconfig:
	cd bsd.rd; ${MAKE} unconfig
	cd floppy; ${MAKE} unconfig
	cd floppyB; ${MAKE} unconfig
#	cd floppyC; ${MAKE} unconfig

.include <bsd.subdir.mk>
