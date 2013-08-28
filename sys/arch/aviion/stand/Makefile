#	$OpenBSD: Makefile,v 1.1.1.1 2006/05/09 19:31:51 miod Exp $

SUBDIR= a2coff

.if ${MACHINE} == "aviion"
SUBDIR+= libprom libsa libz netboot
.endif

.include <bsd.subdir.mk>
