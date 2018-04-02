#	$OpenBSD: Makefile.inc,v 1.8 2011/07/22 09:30:54 espie Exp $

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/mbr ${RELEASEDIR}
	cp -p ${DESTDIR}/usr/mdec/xxboot ${RELEASEDIR}
	cp -p ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}

MDEXT=	mbr bsd.rd xxboot boot miniroot${OSrev}.fs
