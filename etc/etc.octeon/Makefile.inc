#	$OpenBSD: Makefile.inc,v 1.4 2015/06/13 18:22:57 jasper Exp $

KERNELS += GENERIC.MP bsd.mp

bootblocks:
	# None yet.

MDEXT=	boot bsd.rd miniroot${OSrev}.fs
