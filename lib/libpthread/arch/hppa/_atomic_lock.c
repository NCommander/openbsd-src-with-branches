/*	$OpenBSD: _atomic_lock.c,v 1.1 1998/12/21 22:54:55 mickey Exp $	*/
/*
 * Atomic lock for hppa
 */
#include "spinlock.h"

int
_atomic_lock(volatile _spinlock_lock_t *lock)
{
	register register_t old;

	__asm__("ldcws 0(%1), %0" : "=r" (old): "r" (lock));

	return (old == _SPINLOCK_LOCKED);
}
