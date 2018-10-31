#	$OpenBSD$

AWK=	awk

radeon_devlist.h: ../drm_pciids.h devlist.awk
	/bin/rm -f radeon_devlist.h
	${AWK} -f devlist.awk ../drm_pciids.h > radeon_devlist.h
