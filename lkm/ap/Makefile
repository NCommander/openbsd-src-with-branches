#	$OpenBSD: Makefile,v 1.2 1996/06/02 04:53:33 tholo Exp $

LKM=	ap
SRCS+=	aperture.c lkm.c version.c
NOMAN=	noman

.ifmake !(install)
SUBDIR=	aptest
.endif

.include <bsd.lkm.mk>
