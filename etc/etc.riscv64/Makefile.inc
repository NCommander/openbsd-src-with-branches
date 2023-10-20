#	$OpenBSD: Makefile.inc,v 1.1 2021/04/28 03:26:25 drahn Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/BOOTRISCV64.EFI ${RELEASEDIR}

MDEXT=	bsd.rd BOOTRISCV64.EFI miniroot${OSrev}.img
