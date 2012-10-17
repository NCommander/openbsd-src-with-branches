#	$OpenBSD: Makefile.inc,v 1.20 2011/04/15 03:11:38 deraadt Exp $

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/bootxx ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/netboot ${RELEASEDIR}

MDEXT=	bsd.rd cd${OSrev}.iso \
	floppy${OSrev}.fs floppyB${OSrev}.fs \
	boot bootxx netboot
