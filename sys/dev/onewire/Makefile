# $OpenBSD$

AWK?=	awk

onewiredevs.h onewiredevs_data.h: onewiredevs devlist2h.awk
	/bin/rm -f onewiredevs.h onewiredevs_data.h
	${AWK} -f devlist2h.awk onewiredevs
