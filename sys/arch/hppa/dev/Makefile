#	$OpenBSD: Makefile,v 1.2 1998/12/04 01:57:18 mickey Exp $

AWK?=	awk

cpudevs.h cpudevs_data.h: cpudevs devlist2h.awk
	/bin/rm -f cpudevs.h cpudevs_data.h
	${AWK} -f devlist2h.awk cpudevs

