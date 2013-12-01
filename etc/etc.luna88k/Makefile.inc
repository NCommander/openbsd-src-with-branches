#	$OpenBSD: Makefile.inc,v 1.7 2012/03/01 14:04:03 aoyama Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${RELEASEDIR}/

MDEXT=	bsd.rd \
	boot
