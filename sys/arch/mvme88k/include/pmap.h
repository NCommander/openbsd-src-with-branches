/* $OpenBSD: pmap.h,v 1.35 2004/05/20 09:20:42 kettenis Exp $ */
/* public domain */

#ifndef	_MVME88K_PMAP_H_
#define	_MVME88K_PMAP_H_

#include <m88k/pmap.h>

#ifdef	_KERNEL
vaddr_t	pmap_bootstrap_md(vaddr_t);
#endif

#endif	_MVME88K_PMAP_H_
