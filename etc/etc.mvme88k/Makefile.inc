#	$OpenBSD: Makefile.inc,v 1.13 2011/04/15 03:11:38 deraadt Exp $

KERNELS += CONFIG.MP bsd.mp

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${RELEASEDIR}/

MDEXT=	bsd.rd \
	bootsd bootst bootxx installboot netboot stboot tftpboot
