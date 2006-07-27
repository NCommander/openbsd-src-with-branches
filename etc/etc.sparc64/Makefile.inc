#	$OpenBSD: Makefile.inc,v 1.2 2002/05/13 07:22:43 espie Exp $

.ifdef DESTDIR
snap_md: bsd bootblocks distrib
	cp ${.CURDIR}/../sys/arch/sparc64/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/sparc64/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/sparc64/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/*boot* ${DESTDIR}/snapshot

.PHONY: bsd bootblocks

.endif	# DESTDIR check
