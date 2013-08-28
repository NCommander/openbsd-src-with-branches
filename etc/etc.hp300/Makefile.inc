#	$OpenBSD: Makefile.inc,v 1.22 2011/04/15 03:11:38 deraadt Exp $

bootblocks:
	cp ${DESTDIR}/usr/mdec/uboot.lif ${RELEASEDIR}/SYS_UBOOT
	cp ${DESTDIR}/usr/mdec/cdboot.lif ${RELEASEDIR}/SYS_CDBOOT

MDEXT=	bsd.rd SYS_UBOOT SYS_CDBOOT
