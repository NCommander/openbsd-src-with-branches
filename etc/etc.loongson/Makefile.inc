#	$OpenBSD: Makefile.inc,v 1.4 2011/04/15 03:11:38 deraadt Exp $

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}

MDEXT=	bsd.rd boot miniroot${OSrev}.fs
