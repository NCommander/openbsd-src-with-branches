#	$OpenBSD: Makefile,v 1.11 2017/01/27 17:59:09 natano Exp $

# preserve order: 'cdfs' depends on 'miniroot'
SUBDIR+= ramdisk ramdiskB miniroot cdfs

.if	make(obj) || make(cleandir) || make(clean)
SUBDIR+= iso
.endif

unconfig:
	cd ramdisk; ${MAKE} unconfig
	cd ramdiskB; ${MAKE} unconfig
	cd miniroot; ${MAKE} unconfig

.include <bsd.subdir.mk>
