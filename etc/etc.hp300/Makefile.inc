#	$OpenBSD: Makefile.inc,v 1.16 1999/08/18 17:27:14 downsj Exp $
#	etc.hp300/Makefile.inc -- hp300-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd bsd.dl bootblocks distrib
	cp ${.CURDIR}/../sys/arch/hp300/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd
	cp ${.CURDIR}/../sys/arch/hp300/compile/DISKLESS/bsd \
	    ${DESTDIR}/snapshot/bsd.dl

bsd:
	cd ${.CURDIR}/../sys/arch/hp300/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/hp300/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bsd.dl:
	cd ${.CURDIR}/../sys/arch/hp300/conf && config DISKLESS
	cd ${.CURDIR}/../sys/arch/hp300/compile/DISKLESS && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/uboot.lif ${DESTDIR}/snapshot/SYS_UBOOT
	cp ${DESTDIR}/usr/mdec/cdboot.lif ${DESTDIR}/snapshot/SYS_CDBOOT

.PHONY: bsd bsd.dl bootblocks

.endif	# DESTDIR check
