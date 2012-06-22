#	$OpenBSD: Makefile,v 1.9 2007/10/18 18:16:20 deraadt Exp $

# preserve order: 'miniroot' depends on 'bsd.rd', 'cdfs' depends on 'miniroot'
SUBDIR+= ramdisk ramdiskB bsd.rd miniroot cdfs

.if	make(obj) || make(cleandir) || make(clean)
SUBDIR+= iso
.endif

unconfig:
	cd ramdisk; ${MAKE} unconfig
	cd ramdiskB; ${MAKE} unconfig

.include <bsd.subdir.mk>
