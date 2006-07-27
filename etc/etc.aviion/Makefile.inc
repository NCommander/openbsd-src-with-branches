#	$OpenBSD: Makefile.inc,v 1.1 2006/05/09 21:17:13 miod Exp $

.ifdef DESTDIR
snap_md: bsd bootblocks distrib
	cp ${.CURDIR}/../sys/arch/aviion/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/aviion/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/aviion/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${DESTDIR}/snapshot/

.PHONY: bsd bootblocks

.endif	# DESTDIR check
