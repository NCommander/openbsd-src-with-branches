#	$OpenBSD: Makefile.inc,v 1.16 2017/09/01 04:40:40 jsg Exp $

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/BOOTARM.EFI ${RELEASEDIR}

MDEXT=	bsd.rd BOOTARM.EFI \
	miniroot-am335x-${OSrev}.img \
	miniroot-cubie-${OSrev}.img miniroot-cubox-${OSrev}.img \
	miniroot-nitrogen-${OSrev}.img miniroot-panda-${OSrev}.img \
	miniroot-wandboard-${OSrev}.img
