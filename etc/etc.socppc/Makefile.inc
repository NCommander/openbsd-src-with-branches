#	$OpenBSD: Makefile.inc,v 1.6 2011/04/15 03:11:38 deraadt Exp $

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}/boot
	cp ${DESTDIR}/usr/mdec/boot.elf ${RELEASEDIR}/boot.elf

MDEXT=	bsd.bin bsd.rd boot boot.elf miniroot${OSrev}.fs
