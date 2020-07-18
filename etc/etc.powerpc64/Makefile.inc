#	$OpenBSD: Makefile.inc,v 1.2 2020/07/05 21:57:23 kettenis Exp $

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}

MDEXT=	boot bsd.rd miniroot${OSrev}.img
