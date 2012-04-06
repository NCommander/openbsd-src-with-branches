#	$OpenBSD: Makefile.inc,v 1.7 2011/04/15 03:11:38 deraadt Exp $

bootblocks:
	cp ${DESTDIR}/usr/mdec/mbr ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/xxboot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}

MDEXT=	mbr bsd.rd xxboot boot miniroot${OSrev}.fs
