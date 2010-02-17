#	$OpenBSD: Makefile.inc,v 1.1 2010/01/23 19:26:45 matthieu Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/loongson/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd
bsd:
	cd ${.CURDIR}/../sys/arch/loongson/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/loongson/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}

MDEXT=	bsd bsd.rd

.PHONY: bsd bootblocks
