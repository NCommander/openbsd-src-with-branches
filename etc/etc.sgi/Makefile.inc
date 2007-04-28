#	$OpenBSD: Makefile.inc,v 1.3 2006/07/24 18:11:47 deraadt Exp $

.ifdef DESTDIR
snap_md: bsd bootblocks distrib
	cp ${.CURDIR}/../sys/arch/sgi/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/sgi/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/sgi/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	#none needed

.PHONY: bsd bootblocks

.endif	# DESTDIR check
