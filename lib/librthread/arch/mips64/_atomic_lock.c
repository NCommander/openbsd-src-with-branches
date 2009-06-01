/*	$OpenBSD: _atomic_lock.c,v 1.2 2009/02/15 17:48:58 deraadt Exp $	*/

/*
 * Atomic lock for mips
 * Written by Miodrag Vallat <miod@openbsd.org> - placed in the public domain.
 */

#include "spinlock.h"

int
_atomic_lock(volatile _spinlock_lock_t *lock)
{
	_spinlock_lock_t old;

	__asm__ __volatile__ (
	"1:	ll	%0,	0(%1)\n"
	"	sc	%2,	0(%1)\n"
	"	beqz	%2,	1b\n"
	"	 nop\n" :
		"=r"(old) :
		"r"(lock), "r"(_SPINLOCK_LOCKED) : "memory");

	return (old != _SPINLOCK_UNLOCKED);
}
