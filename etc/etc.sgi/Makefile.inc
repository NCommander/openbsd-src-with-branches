#	$OpenBSD: Makefile.inc,v 1.1 2004/08/06 21:58:22 pefo Exp $
#	etc.sgi/Makefile.inc -- sgi-specific etc Makefile targets

.ifdef DESTDIR

snap_md: bsd distrib1
	cp ${.CURDIR}/../sys/arch/sgi/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/sgi/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/sgi/compile/GENERIC && \
	    make clean && make

distrib1:
	${MAKE} distrib

.PHONY: bsd distrib1

.endif
