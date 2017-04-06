#	$OpenBSD: Makefile.inc,v 1.13 2014/11/26 08:21:03 stsp Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp ${DESTDIR}/usr/mdec/{bootblk,ofwboot,ofwboot.net} ${RELEASEDIR}

MDEXT=	bsd.rd cd${OSrev}.iso \
	miniroot${OSrev}.fs floppy${OSrev}.fs floppyB${OSrev}.fs \
	bootblk ofwboot ofwboot.net
