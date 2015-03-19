#	$OpenBSD: Makefile.inc,v 1.11 2011/04/15 03:11:38 deraadt Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp ${DESTDIR}/usr/mdec/xxboot ${RELEASEDIR}

MDEXT=	bsd.rd cd${OSrev}.iso lif${OSrev}.fs xxboot
