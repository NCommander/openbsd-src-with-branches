/*	$OpenBSD: pmap.h,v 1.1 2001/06/26 21:57:47 smurph Exp $	*/

#include <powerpc/pmap.h>

#ifndef	_LOCORE
paddr_t vtophys __P((vaddr_t));
#endif	/* _LOCORE */
