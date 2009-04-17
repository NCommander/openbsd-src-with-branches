#	$OpenBSD: Makefile.inc,v 1.4 2006/07/27 02:51:15 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/mvmeppc/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/mvmeppc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mvmeppc/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/bootxx ${RELEASEDIR}/bootxx
	cp ${DESTDIR}/usr/mdec/bootsd ${RELEASEDIR}/bootsd
	cp ${DESTDIR}/usr/mdec/installboot ${RELEASEDIR}/installboot

MDEXT=	

.PHONY: bsd bootblocks
