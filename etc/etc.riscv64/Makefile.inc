#	$OpenBSD: Makefile.inc,v 1.5 2020/05/17 17:04:28 deraadt Exp $

KERNELS += 

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/BOOTRISCV64.EFI ${RELEASEDIR}

MDEXT=	bsd.rd BOOTRISCV64.EFI miniroot${OSrev}.img
