#	$OpenBSD$

.if	make(obj) || make(clean)
SUBDIR!=find . -type d -maxdepth 1 \! \( -name . -o -name CVS \)
#SUBDIR=GENERIC GENERIC.MP RAMDISK RAMDISKB RAMDISKBIG RAMDISKC
.endif

.include <bsd.subdir.mk>
