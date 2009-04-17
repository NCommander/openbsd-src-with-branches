#	$OpenBSD: Makefile.inc,v 1.1 2008/05/13 19:39:58 kettenis Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/socppc/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/socppc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/socppc/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}/boot

MDEXT=	

.PHONY: bsd bootblocks
