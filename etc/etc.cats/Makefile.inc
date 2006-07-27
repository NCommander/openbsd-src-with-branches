#	$OpenBSD: Makefile.inc,v 1.4 2004/02/09 02:28:23 drahn Exp $

.ifdef DESTDIR
snap_md: bsd bootblocks distrib
	cp ${.CURDIR}/../sys/arch/cats/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/cats/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/cats/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	#none needed

.PHONY: bsd bootblocks

.endif	# DESTDIR check
