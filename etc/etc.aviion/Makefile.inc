#	$OpenBSD: Makefile.inc,v 1.2 2006/07/27 02:53:55 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/aviion/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/aviion/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/aviion/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${RELEASEDIR}/

MDEXT=	

.PHONY: bsd bootblocks
