#	$OpenBSD: Makefile,v 1.6 2003/03/05 23:17:20 deraadt Exp $

SUBDIR+= ../ramdisk ../miniroot cdfs

.if	make(obj)
SUBDIR+= iso
.endif

unconfig:
	cd ramdisk_cd; ${MAKE} unconfig
	cd ramdiskA; ${MAKE} unconfig

.include <bsd.subdir.mk>
