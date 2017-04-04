#	$OpenBSD: Makefile.inc,v 1.24 2015/01/12 17:10:38 deraadt Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/pxeboot ${RELEASEDIR}
	cp -p ${DESTDIR}/usr/mdec/cdboot ${RELEASEDIR}
	cp -p ${DESTDIR}/usr/mdec/cdbr ${RELEASEDIR}

MDEXT=	bsd.rd cd${OSrev}.iso floppy${OSrev}.fs miniroot${OSrev}.fs \
	pxeboot cdboot cdbr
