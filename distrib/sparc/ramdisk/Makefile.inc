#	$OpenBSD$
#	$OpenBSD: Makefile.inc,v 1.4 1997/05/07 12:40:50 niklas Exp $

IMAGESIZE?=		4096 # 2Mb in 512 byte blocks
BOOT_INSTALLBOOT?=	/usr/mdec/bootxx
GZIP?=			gzip
GZIPFLAGS?=		-9v
GZIPEXT?=		.gz
#MAKEFLOPPY=		1
