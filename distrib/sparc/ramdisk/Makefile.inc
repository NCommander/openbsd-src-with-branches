#	$OpenBSD: Makefile.inc,v 1.11 1998/02/18 22:46:51 deraadt Exp $
#	$OpenBSD: Makefile.inc,v 1.11 1998/02/18 22:46:51 deraadt Exp $

IMAGESIZE?=		5352
GZIP?=			gzip
GZIPFLAGS?=		-9v
GZIPEXT?=		.gz
MAKEFLOPPY=		1
RAWLABEL=
BOOT=			${DESTDIR}/usr/mdec/boot
INSTALLBOOT=		${DESTDIR}/usr/mdec/installboot
