#	$OpenBSD: GENERIC.MP,v 1.1.2.1 2001/07/14 10:02:21 ho Exp $
#
#	GENERIC.MP - sample multiprocessor kernel
#

include "arch/i386/conf/GENERIC"

option		MULTIPROCESSOR	# Multiple processor support
option		CPU

cpu*		at mainbus? apid ?
ioapic* 	at mainbus? apid ?
