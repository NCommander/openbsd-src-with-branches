/*	$OpenBSD$	*/

/*
 * Atomic lock for vax
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
