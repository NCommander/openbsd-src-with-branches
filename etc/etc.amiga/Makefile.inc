#
#	etc.amiga/Makefile.inc -- amiga-specific etc Makefile targets
#
#	$OpenBSD$
#	$NetBSD: Makefile.inc,v 1.7 1995/11/28 05:40:21 jtc Exp $

.ifdef DESTDIR
snap_md: bsd-install
	cp ${.CURDIR}/../sys/arch/amiga/compile/GENERIC/bsd.gz \
	    ${DESTDIR}/snapshot/bsd-generic.gz
bsd-install:
	cd ${.CURDIR}/../sys/arch/amiga/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/amiga/compile/GENERIC && \
	    make clean && make depend && make && gzip -c -9 bsd >bsd.gz
.endif	# DESTDIR check
