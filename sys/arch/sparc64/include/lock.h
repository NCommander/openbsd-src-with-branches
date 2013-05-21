/*	$OpenBSD: lock.h,v 1.7 2011/07/02 22:19:16 guenther Exp $	*/

/* public domain */

#ifndef	_MACHINE_LOCK_H_
#define	_MACHINE_LOCK_H_

#include <machine/atomic.h>
#include <machine/ctlreg.h>

#define	rw_cas(p, o, n)		(sparc64_casx(p, o, n) != o)

#endif	/* _MACHINE_LOCK_H_ */
