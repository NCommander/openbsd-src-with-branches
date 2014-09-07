#	$OpenBSD: Makefile.inc,v 1.10 2011/04/15 03:11:38 deraadt Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp ${DESTDIR}/usr/mdec/*boot* ${RELEASEDIR}

MDEXT=	bsd.rd cd${OSrev}.iso \
	miniroot${OSrev}.fs floppy${OSrev}.fs floppyB${OSrev}.fs \
	bootblk installboot ofwboot ofwboot.net
