#	$OpenBSD: Makefile.inc,v 1.11 1997/09/18 17:17:35 deraadt Exp $
#	etc.amiga/Makefile.inc -- amiga-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd distrib

bsd:
	# The GENERIC kernel is built as a side-effect of the miniroot
	cp ${.CURDIR}/../sys/arch/amiga/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

distrib:
	cd ${.CURDIR}/../distrib && \
	    ${MAKE} cleandir && ${MAKE} && ${MAKE} install

.endif	# DESTDIR check
