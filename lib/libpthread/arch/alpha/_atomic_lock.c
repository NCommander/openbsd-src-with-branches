/*	$OpenBSD: _atomic_lock.c,v 1.5 1999/05/26 00:11:27 d Exp $	*/
/*
 * Atomi lock for alpha.
 */

#include "spinlock.h"

/* _atomic lock is implemented in assembler. */

int
_atomic_is_locked(volatile _spinlock_lock_t * lock)
{
	
	return (*lock != _SPINLOCK_UNLOCKED);
}
