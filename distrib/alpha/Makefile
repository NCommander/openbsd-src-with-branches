#	$OpenBSD: Makefile,v 1.8 2007/10/18 18:16:19 deraadt Exp $

SUBDIR= bsd.rd floppy floppyB floppyC cdfs

.if	make(obj) || make(cleandir) || make(clean)
SUBDIR+= iso
.endif

unconfig:
	cd bsd.rd; ${MAKE} unconfig
	cd floppy; ${MAKE} unconfig
	cd floppyB; ${MAKE} unconfig
	cd floppyC; ${MAKE} unconfig
	cd cdfs; ${MAKE} unconfig

.include <bsd.subdir.mk>
