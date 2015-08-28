#	$OpenBSD: Makefile.inc,v 1.15 2011/04/15 03:11:38 deraadt Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp ${DESTDIR}/usr/mdec/ofwboot ${RELEASEDIR}/ofwboot
	cp ${DESTDIR}/usr/mdec/bsd.tbxi ${RELEASEDIR}/bsd.tbxi
	cp ${DESTDIR}/usr/mdec/boot.mac ${RELEASEDIR}/boot.mac

MDEXT=	bsd.rd cd${OSrev}.iso ofwboot bsd.tbxi boot.mac
