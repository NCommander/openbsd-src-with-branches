#	$OpenBSD: Makefile.inc,v 1.1 2013/09/04 16:53:40 patrick Exp $

ALLSOC=IMX OMAP
KERNELS=
MDEXT=

.for SOC in ${ALLSOC}
KERNELS+=	GENERIC-${SOC} bsd.${SOC}
MDEXT+=		bsd.rd.${SOC}
.endfor

bootblocks:

MDEXT+=	miniroot-am335x-${OSrev}.fs miniroot-beagle-${OSrev}.fs \
	miniroot-panda-${OSrev}.fs
