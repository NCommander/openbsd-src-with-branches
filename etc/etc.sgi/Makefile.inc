#	$OpenBSD: Makefile.inc,v 1.4 2006/07/27 02:53:55 deraadt Exp $

kernels: bsd
	cp ${.CURDIR}/../sys/arch/sgi/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/sgi/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/sgi/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

MDEXT=	

.PHONY: bsd

