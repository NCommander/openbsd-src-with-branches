#	$OpenBSD: Makefile,v 1.7 2005/08/09 16:48:11 deraadt Exp $

# preserve order: 'miniroot' depends on 'bsd.rd', 'cdfs' depends on 'miniroot'
SUBDIR+= ramdisk ramdiskB bsd.rd miniroot cdfs

.if	make(obj)
SUBDIR+= iso
.endif

unconfig:
	cd ramdisk_cd; ${MAKE} unconfig
	cd ramdiskA; ${MAKE} unconfig

.include <bsd.subdir.mk>
