#	$OpenBSD$
#	etc.luna88k/Makefile.inc -- luna88k-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd-generic distrib
	cp ${.CURDIR}/../sys/arch/luna88k/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd-generic:
	cd ${.CURDIR}/../sys/arch/luna88k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/luna88k/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && ${MAKE}

.PHONY: bsd-generic

.endif	# DESTDIR check
