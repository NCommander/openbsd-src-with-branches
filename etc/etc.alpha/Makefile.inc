#	$OpenBSD: Makefile.inc,v 1.12 2001/03/05 14:32:10 deraadt Exp $
#	etc.alpha/Makefile.inc -- alpha-specific etc Makefile targets

.ifdef DESTDIR

snap_md: bsd distrib boot
	cp ${.CURDIR}/../sys/arch/alpha/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/alpha/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/alpha/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}


boot:
	cp ${DESTDIR}/usr/mdec/boot ${DESTDIR}/snapshot
	cp ${DESTDIR}/usr/mdec/bootxx ${DESTDIR}/snapshot
	cp ${DESTDIR}/usr/mdec/netboot ${DESTDIR}/snapshot

.PHONY: bsd boot

.endif	# DESTDIR check
