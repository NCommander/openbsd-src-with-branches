#	$OpenBSD$

IMAGESIZE?=		3000 # 1.5Mb & change in 512 byte blocks
GZIP?=			gzip
GZIPFLAGS?=		-9v
GZIPEXT?=		.gz
RAWLABEL=
BOOT=			${DESTDIR}/usr/mdec/ufsboot
INSTALLBOOT=		${DESTDIR}/usr/mdec/installboot
