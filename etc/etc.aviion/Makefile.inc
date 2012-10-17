#	$OpenBSD: Makefile.inc,v 1.5 2011/04/15 03:11:38 deraadt Exp $

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${RELEASEDIR}/

MDEXT=	bsd.rd	# bootblocks?
