#	$OpenBSD: Makefile,v 1.7 1997/04/18 07:27:41 deraadt Exp $

SUBDIR= ramdisk_cd ramdisk ramdiskB

unconfig:
	cd ramdisk_cd; ${MAKE} unconfig
	cd ramdisk; ${MAKE} unconfig
	cd ramdiskB; ${MAKE} unconfig

.include <bsd.subdir.mk>
