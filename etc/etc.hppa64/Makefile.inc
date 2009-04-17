#	$OpenBSD: Makefile.inc,v 1.2 2006/07/27 02:53:55 deraadt Exp $

kernels: bootblocks bsd
	cp ${.CURDIR}/../sys/arch/hppa64/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/hppa64/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/hppa64/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/xxboot ${RELEASEDIR}

MDEXT=	

.PHONY: bootblocks bsd
