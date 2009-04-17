#	$OpenBSD: Makefile.inc,v 1.5 2006/07/27 02:53:55 deraadt Exp $

kernels: bootblocks bsd
	cp ${.CURDIR}/../sys/arch/hppa/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/hppa/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/hppa/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/xxboot ${RELEASEDIR}

MDEXT=	cd${OSrev}.iso xxboot

.PHONY: bsd bootblocks
