#	$OpenBSD$

.if ${MACHINE} == "hppa64"
SUBDIR= libsa libkern libz
.endif
SUBDIR+= mkboot boot
.if ${MACHINE} == "hppa64"
#SUBDIR+= cdboot
.endif

.include <bsd.subdir.mk>
