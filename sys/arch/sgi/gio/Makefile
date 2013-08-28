#	$OpenBSD$
#	$NetBSD: Makefile.giodevs,v 1.5 2008/10/19 22:05:21 apb Exp $

AWK=	awk

giodevs.h giodevs_data.h: giodevs devlist2h.awk
	/bin/rm -f giodevs.h giodevs_data.h
	${AWK} -f devlist2h.awk giodevs
