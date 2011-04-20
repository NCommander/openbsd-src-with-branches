#	$OpenBSD: GENERIC.MP,v 1.9 2008/12/21 18:49:46 kettenis Exp $

include "arch/amd64/conf/GENERIC"

option	MULTIPROCESSOR

cpu*		at mainbus?
