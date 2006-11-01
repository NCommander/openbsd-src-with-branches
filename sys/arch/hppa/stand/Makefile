#	$OpenBSD: Makefile,v 1.2 2000/05/31 19:59:31 mickey Exp $

.if ${MACHINE} == "hppa"
SUBDIR= libsa libkern libz
.endif
SUBDIR+= mkboot boot
.if ${MACHINE} == "hppa"
SUBDIR+= cdboot
.endif

.include <bsd.subdir.mk>
