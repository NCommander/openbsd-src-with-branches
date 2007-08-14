#	$OpenBSD: Makefile,v 1.1 2001/09/01 16:47:03 drahn Exp $

SUBDIR= ramdisk

.if	make(obj)
SUBDIR+= iso
.endif

unconfig:
	cd ramdisk_cd; ${MAKE} unconfig
	cd ramdiskA; ${MAKE} unconfig

.include <bsd.subdir.mk>
