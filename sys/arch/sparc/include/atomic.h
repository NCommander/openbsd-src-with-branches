/*	$OpenBSD: atomic.h,v 1.4 2010/04/21 03:03:24 deraadt Exp $	*/

/* Public Domain */

#ifndef _MACHINE_ATOMIC_H_
#define _MACHINE_ATOMIC_H_

#if defined(_KERNEL)

#include <machine/psl.h>

static __inline void
atomic_setbits_int(__volatile unsigned int *uip, unsigned int v)
{
	int psr;

	psr = getpsr();
	setpsr(psr | PSR_PIL);
	*uip |= v;
	setpsr(psr);
}

static __inline void
atomic_clearbits_int(__volatile unsigned int *uip, unsigned int v)
{
	int psr;

	psr = getpsr();
	setpsr(psr | PSR_PIL);
	*uip &= ~v;
	setpsr(psr);
}

#endif /* defined(_KERNEL) */
#endif /* _MACHINE_ATOMIC_H_ */
