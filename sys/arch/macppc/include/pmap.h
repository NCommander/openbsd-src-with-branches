/*	$OpenBSD: pmap.h,v 1.2 2001/09/10 16:44:52 mickey Exp $	*/

#include <powerpc/pmap.h>

#ifndef	_LOCORE
paddr_t vtophys __P((vaddr_t));
#endif	/* _LOCORE */
