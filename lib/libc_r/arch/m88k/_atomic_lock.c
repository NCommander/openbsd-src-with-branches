/*	$OpenBSD: _atomic_lock.c,v 1.4 1998/12/21 13:03:44 d Exp $	*/
/*
 * Atomic lock for m68k
 */

#include "spinlock.h"

int
_atomic_lock(volatile _spinlock_lock_t *lock)
{
	return (_thread_slow_atomic_lock(lock));
}

int
_atomic_is_locked(volatile _spinlock_lock_t *lock)
{

	return (*lock != _SPINLOCK_UNLOCKED);
}
