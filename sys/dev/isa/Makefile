#	$OpenBSD: Makefile,v 1.3 1996/04/19 16:08:38 niklas Exp $
#	$NetBSD: Makefile,v 1.1 1995/06/18 01:07:04 cgd Exp $

AWK=	awk

pnpdevs.h: pnpdevs devlist2h.awk
	/bin/rm -f pnpdevs.h
	${AWK} -f devlist2h.awk pnpdevs
