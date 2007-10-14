#	$OpenBSD: Makefile,v 1.1 2007/10/07 17:58:51 otto Exp $
#
# Makefile for the ccom part of pcc.
#

.if ${MACHINE_ARCH} == "i386"
SUBDIR+= x86
.else
SUBDIR+= ${MACHINE_ARCH}
.endif

.include <bsd.subdir.mk>
