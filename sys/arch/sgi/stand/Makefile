# $OpenBSD: Makefile,v 1.3 2004/08/23 14:33:34 deraadt Exp $

SUBDIR=	sgivol

.if ${MACHINE} == "sgi"
SUBDIR+=	libsa libkern libz boot
.endif

.include <bsd.subdir.mk>
