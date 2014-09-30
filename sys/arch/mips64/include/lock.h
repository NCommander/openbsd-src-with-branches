/*	$OpenBSD: lock.h,v 1.5 2013/05/21 20:05:30 tedu Exp $	*/

/* public domain */

#ifndef	_MIPS64_LOCK_H_
#define	_MIPS64_LOCK_H_

#include <sys/atomic.h>

#define rw_cas __cpu_cas
static __inline int
__cpu_cas(volatile unsigned long *addr, unsigned long old, unsigned long new)
{
	return (atomic_cas_ulong(addr, old, new) != old);
}

#endif	/* _MIPS64_LOCK_H_ */
