#	$OpenBSD: Makefile.inc,v 1.25 2015/08/29 17:22:41 deraadt Exp $

KERNELS += GENERIC.MP bsd.mp

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}
	cp -p ${DESTDIR}/usr/mdec/bootxx ${RELEASEDIR}
	cp -p ${DESTDIR}/usr/mdec/netboot ${RELEASEDIR}
	cp -p ${DESTDIR}/usr/mdec/netboot.mop ${RELEASEDIR}

MDEXT=	bsd.rd cd${OSrev}.iso \
	miniroot${OSrev}.fs \
	floppy${OSrev}.fs floppyB${OSrev}.fs floppyC${OSrev}.fs \
	boot bootxx netboot netboot.mop
