# $OpenBSD: Makefile,v 1.5 2007/11/26 09:40:35 jsing Exp $

SUBDIR=	sgivol

.if ${MACHINE} == "sgi"
SUBDIR+=	libsa libsa32 libz libz32
SUBDIR+=	boot64 boot32
.endif

.include <bsd.subdir.mk>
