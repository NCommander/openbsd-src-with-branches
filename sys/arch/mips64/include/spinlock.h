/*	$OpenBSD: spinlock.h,v 1.3 2011/03/23 16:54:36 pirofti Exp $	*/
 /* Public domain */

#ifndef _MIPS64_SPINLOCK_H_
#define _MIPS64_SPINLOCK_H_

#define _ATOMIC_LOCK_UNLOCKED	(0)
#define _ATOMIC_LOCK_LOCKED	(1)
typedef int _atomic_lock_t;

#ifndef _KERNEL
int _atomic_lock(volatile _atomic_lock_t *);
#endif

#endif /* !_MIPS64_SPINLOCK_H_ */
