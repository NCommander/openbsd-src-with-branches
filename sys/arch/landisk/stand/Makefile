#	$OpenBSD: Makefile,v 1.3 2006/10/10 05:26:54 miod Exp $

SUBDIR=		mbr xxboot
.if ${MACHINE} == "landisk"
SUBDIR+=	boot
.endif


.include <bsd.subdir.mk>
