#	$OpenBSD$

AWK=	awk

xbowdevs.h xbowdevs_data.h: xbowdevs devlist2h.awk
	/bin/rm -f xbowdevs.h xbowdevs_data.h
	${AWK} -f devlist2h.awk xbowdevs
