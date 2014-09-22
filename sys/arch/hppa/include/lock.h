/*	$OpenBSD: lock.h,v 1.7 2014/03/29 18:09:29 guenther Exp $	*/

/* public domain */

#ifndef	_MACHINE_LOCK_H_
#define	_MACHINE_LOCK_H_

#include <machine/atomic.h>

#define rw_cas(p, o, n) (atomic_cas_ulong(p, o, n) != o)

#endif	/* _MACHINE_LOCK_H_ */
