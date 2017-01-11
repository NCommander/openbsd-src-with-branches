#	$OpenBSD: Makefile.inc,v 1.14 2016/05/29 07:38:01 jsg Exp $

bootblocks:
	cp ${DESTDIR}/usr/mdec/BOOTAA64.EFI ${RELEASEDIR}

MDEXT=	bsd.rd BOOTAA64.EFI
