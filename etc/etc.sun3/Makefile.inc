#	$OpenBSD: Makefile.inc,v 1.4 2000/06/19 01:49:30 miod Exp $
#	etc.sun3/Makefile.inc -- sun3-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd bootblocks distrib
	cp ${.CURDIR}/../sys/arch/sun3/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/sun3/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/sun3/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/*boot* ${DESTDIR}/snapshot

.PHONY: bsd bootblocks

.endif	# DESTDIR check
