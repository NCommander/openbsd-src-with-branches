#
#	etc.amiga/Makefile.inc -- amiga-specific etc Makefile targets
#
#	$OpenBSD: Makefile.inc,v 1.8 1997/05/18 15:28:13 niklas Exp $
#	$NetBSD: Makefile.inc,v 1.7 1995/11/28 05:40:21 jtc Exp $

.ifdef DESTDIR
snap_md: miniroot ramdisk

miniroot:
	# The GENERIC kernel is built as a side-effect of the miniroot
	cp ${.CURDIR}/../sys/arch/amiga/compile/GENREIC/bsd \
	    ${DESTDIR}/snapshot/bsd
	cd ${.CURDIR}/../distrib/miniroot && \
	    ${MAKE} cleandir && ${MAKE} && ${MAKE} install

ramdisk:
	cd ${.CURDIR}/../distrib/ramdisk && \
	    ${MAKE} cleandir && ${MAKE} && ${MAKE} install

.endif	# DESTDIR check
