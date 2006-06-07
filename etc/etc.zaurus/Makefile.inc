#	$OpenBSD: Makefile.inc,v 1.4 2006/06/06 03:41:51 drahn Exp $
#	etc.zaurus/Makefile.inc -- zaurus-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd bootblocks distrib
	cp ${.CURDIR}/../sys/arch/zaurus/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/zaurus/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/zaurus/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/zboot ${DESTDIR}/snapshot
	cp ${DESTDIR}/usr/mdec/zbsdmod.o ${DESTDIR}/snapshot

.PHONY: bsd bootblocks

.endif	# DESTDIR check
