#	$OpenBSD$

.if	make(obj) || make(clean)
SUBDIR!=find . -type d -maxdepth 1 \! \( -name . -o -name CVS \)
#SUBDIR=GENERIC RAMDISK
.endif

.include <bsd.subdir.mk>
