#	$OpenBSD: Makefile.inc,v 1.2 1997/01/21 12:02:57 graichen Exp $
#
#	etc.arc/Makefile.inc -- arc-specific etc Makefile targets

.ifdef DESTDIR

snap_md: binutils bsd

binutils:
	cd ${DESTDIR} && tar cf - usr/*openbsd* \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/usr.binutils.tar${GZIPEXT}

bsd:
	cd ${.CURDIR}/../sys/arch/arc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/arc/compile/GENERIC && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/arc/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

.endif
