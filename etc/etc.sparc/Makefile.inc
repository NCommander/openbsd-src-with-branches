#	$OpenBSD: Makefile.inc,v 1.20 2011/06/15 21:36:50 miod Exp $

bootblocks:
	cp ${DESTDIR}/usr/mdec/*boot* ${RELEASEDIR}
	${DESTDIR}/usr/mdec/binstall net /tmp && \
	    mv /tmp/boot.sparc.openbsd ${RELEASEDIR}/boot.net

MDEXT=	bsd.rd cd${OSrev}.iso \
	floppy${OSrev}.fs miniroot${OSrev}.fs \
	boot boot.net bootxx installboot
