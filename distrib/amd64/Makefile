#	$OpenBSD: Makefile,v 1.3 2007/08/14 18:57:32 deraadt Exp $

SUBDIR= ramdisk_cd ramdiskA cdfs

.if	make(obj) || make(cleandir) || make(clean)
SUBDIR+= iso
.endif

unconfig:
	cd ramdisk_cd; ${MAKE} unconfig
	cd ramdiskA; ${MAKE} unconfig

.include <bsd.subdir.mk>
