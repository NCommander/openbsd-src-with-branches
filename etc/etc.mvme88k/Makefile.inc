#	$OpenBSD: Makefile.inc,v 1.8 2007/11/09 18:15:22 miod Exp $

kernels: bsd bsd.mp bootblocks
	cp ${.CURDIR}/../sys/arch/mvme88k/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd
	cp ${.CURDIR}/../sys/arch/mvme88k/compile/GENERIC.MP/bsd \
	    ${RELEASEDIR}/bsd.mp

bsd:
	cd ${.CURDIR}/../sys/arch/mvme88k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mvme88k/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && ${MAKE}

bsd.mp:
	cd ${.CURDIR}/../sys/arch/mvme88k/conf && config GENERIC.MP
	cd ${.CURDIR}/../sys/arch/mvme88k/compile/GENERIC.MP && \
	    ${MAKE} clean && ${MAKE} depend && ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${RELEASEDIR}/

MDEXT=	

.PHONY: bsd bsd.mp bootblocks
