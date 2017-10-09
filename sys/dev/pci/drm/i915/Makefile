#	$OpenBSD$

AWK=	awk

i915_devlist.h: i915_drv.c devlist.awk
	/bin/rm -f i915_devlist.h
	${AWK} -f devlist.awk i915_drv.c > i915_devlist.h
