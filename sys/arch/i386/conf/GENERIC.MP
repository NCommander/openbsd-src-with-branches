#	$OpenBSD: SIMPLE,v 1.1.2.3 2001/04/18 16:07:09 niklas Exp $
#
#	GENERIC.MP - sample multiprocessor kernel
#

include "arch/i386/conf/GENERIC"

option		MULTIPROCESSOR	# Multiple processor support
option		CPU

ioapic* 	at mainbus? apid ?
