#	$OpenBSD: Makefile.inc,v 1.10 2011/04/15 03:11:38 deraadt Exp $

bootblocks:
	cp ${DESTDIR}/usr/mdec/zboot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/zbsdmod.o ${RELEASEDIR}

MDEXT=	bsd.rd zboot zbsdmod.o openbsd${OSrev}_arm.ipk Packages
