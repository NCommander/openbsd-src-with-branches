#	$OpenBSD: Makefile.inc,v 1.3 2002/02/15 01:33:31 deraadt Exp $
#
#	etc.vax/Makefile.inc -- vax-specific etc Makefile targets
#

.ifdef DESTDIR
snap_md: bsd bootblocks distrib
	cp ${.CURDIR}/../sys/arch/vax/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/vax/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/vax/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${DESTDIR}/snapshot
	mopa.out ${DESTDIR}/usr/mdec/boot ${DESTDIR}/snapshot/boot.mop

.PHONY: bsd bootblocks

.endif  # DESTDIR check
