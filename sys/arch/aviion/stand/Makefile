#	$OpenBSD: Makefile,v 1.7 2013/10/15 13:28:09 miod Exp $

SUBDIR= a2coff installboot vdmtool

.if ${MACHINE} == "aviion"
SUBDIR+=	libprom libsa libz
SUBDIR+=	boot
.endif

.include <bsd.subdir.mk>
