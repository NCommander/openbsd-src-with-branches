#	$OpenBSD: Makefile.inc,v 1.3 1997/05/15 08:46:13 deraadt Exp $
#	$OpenBSD: Makefile.inc,v 1.3 1997/05/15 08:46:13 deraadt Exp $

IMAGESIZE?=		5120 # 2.5Mb in 512 byte blocks
BOOT_INSTALLBOOT?=	/usr/mdec/bootxx
GZIP?=			gzip
GZIPFLAGS?=		-9v
GZIPEXT?=		.gz
#MAKEFLOPPY=		1
