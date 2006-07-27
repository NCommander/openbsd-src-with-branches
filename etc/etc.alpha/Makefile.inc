#	$OpenBSD: Makefile.inc,v 1.13 2002/05/13 07:22:42 espie Exp $

.ifdef DESTDIR
snap_md: bsd bootlocks distrib
	cp ${.CURDIR}/../sys/arch/alpha/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/alpha/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/alpha/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${DESTDIR}/snapshot
	cp ${DESTDIR}/usr/mdec/bootxx ${DESTDIR}/snapshot
	cp ${DESTDIR}/usr/mdec/netboot ${DESTDIR}/snapshot

.PHONY: bsd bootblocks

.endif	# DESTDIR check
