#	$OpenBSD: Makefile,v 1.3 1996/06/03 04:02:22 mickey Exp $

LKM=	ap
SRCS+=	aperture.c lkm.c version.c
MAN=	ap.4
MLINKS=	ap.4 xf86.4

.ifmake !(install)
SUBDIR=	aptest
.endif

.include <bsd.lkm.mk>
