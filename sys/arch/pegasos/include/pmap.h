/*	$OpenBSD: pmap.h,v 1.1.2.1 2003/10/10 17:04:14 drahn Exp $	*/

#include <powerpc/pmap.h>

#ifndef	_LOCORE
paddr_t vtophys(vaddr_t);
#endif	/* _LOCORE */
