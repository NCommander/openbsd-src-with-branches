#	$OpenBSD: Makefile.inc,v 1.13 2011/04/15 03:11:38 deraadt Exp $

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${RELEASEDIR}/

MDEXT=	bsd.rd \
	installboot netboot rboot sboot stboot bootsd bootst bootxx
