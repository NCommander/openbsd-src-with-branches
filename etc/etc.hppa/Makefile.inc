#	$OpenBSD: Makefile.inc,v 1.1 1999/11/30 21:25:38 mickey Exp $

.ifdef DESTDIR
snap_md: bsd distrib
	cp ${.CURDIR}/../sys/arch/hppa/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/hppa/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/hppa/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

.PHONY: bsd

.endif	# DESTDIR check
