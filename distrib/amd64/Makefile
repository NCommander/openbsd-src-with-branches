#	$OpenBSD$

SUBDIR= ramdisk_cd # ramdiskA ramdiskB ramdiskC cdfs

unconfig:
	cd ramdisk_cd; ${MAKE} unconfig
	#cd ramdiskA; ${MAKE} unconfig
	#cd ramdiskB; ${MAKE} unconfig
	#cd ramdiskC; ${MAKE} unconfig

.include <bsd.subdir.mk>
