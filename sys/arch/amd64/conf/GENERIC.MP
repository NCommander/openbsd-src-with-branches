#	$OpenBSD: GENERIC.MP,v 1.5 2004/10/22 22:10:32 grange Exp $

include "arch/amd64/conf/GENERIC"

option	MULTIPROCESSOR
option	MPBIOS

cpu*		at mainbus?
ioapic*		at mainbus?

aapic*		at pci?			# AMD 8131 IO apic
