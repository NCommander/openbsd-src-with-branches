#	$OpenBSD: Makefile.inc,v 1.9 1998/09/23 07:27:06 todd Exp $
#	etc.sparc64/Makefile.inc -- sparc64-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd bootblocks distrib
	cp ${.CURDIR}/../sys/arch/sparc64/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/sparc64/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/sparc64/compile/GENERIC && \
	    make clean && make depend && make

bootblocks:
	cp ${DESTDIR}/usr/mdec/*boot* ${DESTDIR}/snapshot

distrib:
	cd ${.CURDIR}/../distrib && make cleandir && \
	    make && make install

.endif	# DESTDIR check
