#	$OpenBSD: Makefile.inc,v 1.6 2011/07/22 09:30:54 espie Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${RELEASEDIR}/

MDEXT=	# bsd.rd	# bootblocks?
