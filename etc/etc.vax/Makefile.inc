#	$OpenBSD: Makefile.inc,v 1.11 2011/04/15 03:11:38 deraadt Exp $

bootblocks:
	mopa.out ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}/boot.mop

MDEXT=	bsd.rd boot.mop cd${OSrev}.iso floppy${OSrev}.fs
