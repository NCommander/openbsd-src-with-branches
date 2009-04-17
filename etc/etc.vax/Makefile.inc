#	$OpenBSD: Makefile.inc,v 1.6 2006/07/27 01:58:21 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/vax/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/vax/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/vax/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	mopa.out ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}/boot.mop

MDEXT=	boot.mop

.PHONY: bsd bootblocks
