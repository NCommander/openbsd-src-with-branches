#	$OpenBSD: Makefile.inc,v 1.2 2002/05/13 07:22:42 espie Exp $

.ifdef DESTDIR
snap_md: boot bsd distrib
	cp ${.CURDIR}/../sys/arch/hppa/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/hppa/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/hppa/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}
boot:
	cp ${DESTDIR}/usr/mdec/xxboot ${DESTDIR}/snapshot

.PHONY: bsd

.endif	# DESTDIR check
