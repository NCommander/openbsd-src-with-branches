#	$OpenBSD: Makefile.inc,v 1.2 2006/07/27 02:53:55 deraadt Exp $

kernels: bsd
	cp ${.CURDIR}/../sys/arch/luna88k/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/luna88k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/luna88k/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && ${MAKE}

.PHONY: bsd

