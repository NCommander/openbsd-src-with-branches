#	$OpenBSD: Makefile,v 1.1.1.1.8.1 2001/04/18 16:06:52 niklas Exp $

.if ${MACHINE} == "hppa"
SUBDIR= libsa libkern libz
.endif
SUBDIR+= mkboot boot
.if ${MACHINE} == "hppa"
SUBDIR+= cdboot
.endif

.include <bsd.subdir.mk>
