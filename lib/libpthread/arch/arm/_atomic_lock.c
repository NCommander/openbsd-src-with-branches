/*	$OpenBSD: _atomic_lock.c,v 1.1 2004/01/20 03:11:54 drahn Exp $	*/
/*
 * Atomic lock for powerpc
 */

#include "spinlock.h"

int
_atomic_lock(volatile _spinlock_lock_t *lock)
{
	_spinlock_lock_t old;

	__asm__("swp %0, %2, [%1]"
		: "=r" (old), "=r" (lock)
		: "r" (_SPINLOCK_LOCKED), "1" (lock) );

	return (old != _SPINLOCK_UNLOCKED);
}
