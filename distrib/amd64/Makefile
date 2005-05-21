#	$OpenBSD: Makefile,v 1.1 2004/02/07 21:16:47 deraadt Exp $

SUBDIR= ramdisk_cd ramdiskA cdfs

unconfig:
	cd ramdisk_cd; ${MAKE} unconfig
	cd ramdiskA; ${MAKE} unconfig

.include <bsd.subdir.mk>
