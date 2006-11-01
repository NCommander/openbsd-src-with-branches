#	$OpenBSD: Makefile.inc,v 1.1 2004/04/21 18:23:44 miod Exp $

.ifdef DESTDIR
snap_md: bsd bootblocks distrib
	cp ${.CURDIR}/../sys/arch/luna88k/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/luna88k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/luna88k/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && ${MAKE}

bootblocks:
	#none needed

.PHONY: bsd bootblocks

.endif	# DESTDIR check
