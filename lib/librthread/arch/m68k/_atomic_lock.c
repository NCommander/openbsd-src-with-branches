/*	$OpenBSD: _atomic_lock.c,v 1.3 2008/10/02 23:29:26 deraadt Exp $	*/
/* David Leonard, <d@csee.uq.edu.au>. Public domain. */

/*
 * Atomic lock for m68k
 */

#include <machine/spinlock.h>

int
_atomic_lock(volatile _atomic_lock_t *lock)
{
	_atomic_lock_t old;

	/*
	 * The Compare And Swap instruction (mc68020 and above)
	 * compares its first operand with the memory addressed by
	 * the third. If they are the same value, the second operand
	 * is stored at the address. Otherwise the 1st operand (register)
	 * is loaded with the contents of the 3rd operand.
	 *
	 *      old = 0;
	 *	CAS(old, 1, *lock);
	 *	if (old == 1) { lock was acquired }
	 *
	 * From the MC68030 User's Manual (Motorola), page `3-13':
	 *    CAS Dc,Du,<ea>:
	 *	(<ea> - Dc) -> cc;
	 *	if Z then Du -> <ea>
	 *	else      <ea> -> Dc;
	 */
	old = _ATOMIC_LOCK_UNLOCKED;
	__asm__("casl %0, %2, %1" : "=d" (old), "=m" (*lock)
				  : "d" (_ATOMIC_LOCK_LOCKED),
				    "0" (old),  "1" (*lock)
				  : "cc");
	return (old != _ATOMIC_LOCK_UNLOCKED);
}
