#	$OpenBSD: Makefile.inc,v 1.5 1997/05/24 10:46:21 johns Exp $
#	$OpenBSD: Makefile.inc,v 1.5 1997/05/24 10:46:21 johns Exp $

IMAGESIZE?=		4352 # 2Mb & change in 512 byte blocks
GZIP?=			gzip
GZIPFLAGS?=		-9v
GZIPEXT?=		.gz
MAKEFLOPPY=		1
RAWLABEL=
BOOT=			${DESTDIR}/usr/mdec/zboot
