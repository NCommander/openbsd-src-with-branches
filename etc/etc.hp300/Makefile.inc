#	$OpenBSD: Makefile.inc,v 1.17 2002/05/13 07:22:42 espie Exp $
#	etc.hp300/Makefile.inc -- hp300-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd bootblocks distrib
	cp ${.CURDIR}/../sys/arch/hp300/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/hp300/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/hp300/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/uboot.lif ${DESTDIR}/snapshot/SYS_UBOOT
	cp ${DESTDIR}/usr/mdec/cdboot.lif ${DESTDIR}/snapshot/SYS_CDBOOT

.PHONY: bsd bootblocks

.endif	# DESTDIR check
