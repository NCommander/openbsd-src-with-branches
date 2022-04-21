#	$OpenBSD$

AWK=	awk

sdmmcdevs.h: sdmmcdevs devlist2h.awk
	/bin/rm -f sdmmcdevs.h
	${AWK} -f devlist2h.awk sdmmcdevs
