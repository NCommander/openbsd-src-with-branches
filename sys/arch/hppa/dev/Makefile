#	$OpenBSD$

AWK?=	awk

boards.h boards_data.h cpudevs.h cpudevs_data.h: cpudevs devlist2h.awk
	/bin/rm -f cpudevs.h cpudevs_data.h
	${AWK} -f devlist2h.awk cpudevs

