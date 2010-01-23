#	$OpenBSD: Makefile.inc,v 1.12 2009/04/21 09:12:27 deraadt Exp $

kernels: bsd
	cp ${.CURDIR}/../sys/arch/loongson/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd
bsd:
	cd ${.CURDIR}/../sys/arch/loongson/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/loongson/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

MDEXT=	bsd bsd.rd

.PHONY: bsd

