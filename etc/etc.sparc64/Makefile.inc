#	$OpenBSD: Makefile.inc,v 1.1 2001/09/11 15:51:50 jason Exp $
#	etc.sparc64/Makefile.inc -- sparc64-specific etc Makefile targets

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
