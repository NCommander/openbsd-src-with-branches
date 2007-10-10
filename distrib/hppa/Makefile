#	$OpenBSD: Makefile,v 1.1 2003/01/30 21:05:00 mickey Exp $

SUBDIR=ramdisk

.if	make(obj)
SUBDIR+= iso
.endif

unconfig:
	cd ramdisk_cd; ${MAKE} unconfig
	cd ramdiskA; ${MAKE} unconfig

.include <bsd.subdir.mk>
