/*	$OpenBSD: biosmem.S,v 1.1 1997/03/31 23:08:36 mickey Exp $	*/

#include <machine/asm.h>
#define	_LOCORE
#include "biosdev.h"
#undef	_LOCORE

ENTRY(biosmem)
	BIOSINT(0x12)
	ret
