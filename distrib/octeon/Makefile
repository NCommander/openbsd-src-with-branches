#	$OpenBSD: Makefile,v 1.3 2019/04/30 17:46:05 deraadt Exp $

SUBDIR= boot ramdisk

unconfig:
	cd boot; ${MAKE} unconfig
	cd ramdisk; ${MAKE} unconfig

.include <bsd.subdir.mk>
