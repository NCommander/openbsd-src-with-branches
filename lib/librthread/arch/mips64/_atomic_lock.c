/*	$OpenBSD: _atomic_lock.c,v 1.7 2013/06/03 16:19:45 miod Exp $	*/

/*
 * Atomic lock for mips
 * Written by Miodrag Vallat <miod@openbsd.org> - placed in the public domain.
 */

#include <machine/spinlock.h>

int
_atomic_lock(volatile _atomic_lock_t *lock)
{
	_atomic_lock_t old;

	__asm__ volatile (
	".set	noreorder\n"
	"1:	ll	%0,	0(%1)\n"
	"	sc	%2,	0(%1)\n"
	"	beqz	%2,	1b\n"
	"	 addi	%2,	$0, %3\n"
	".set	reorder\n"
		: "=&r"(old)
		: "r"(lock), "r"(_ATOMIC_LOCK_LOCKED), "i"(_ATOMIC_LOCK_LOCKED)
		: "memory");

	return (old != _ATOMIC_LOCK_UNLOCKED);
}
