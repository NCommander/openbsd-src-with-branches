#	$OpenBSD$
#
#	GENERIC.MP - sample multiprocessor kernel
#

include "arch/i386/conf/GENERIC"

option		MULTIPROCESSOR	# Multiple processor support
option		CPU

cpu*		at mainbus? apid ?
ioapic* 	at mainbus? apid ?
