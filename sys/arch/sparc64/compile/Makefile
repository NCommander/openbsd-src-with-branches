#	$OpenBSD$

.if	make(obj) || make(clean)
SUBDIR!=find . -type d -maxdepth 1 \! \( -name . -o -name CVS \)
#SUBDIR=GENERIC GENERIC.MP RAMDISK RAMDISKU1 RAMDISKU5
.endif

.include <bsd.subdir.mk>

