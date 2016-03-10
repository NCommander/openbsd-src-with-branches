/*	$OpenBSD: spinlock.h,v 1.1 2004/04/26 12:34:05 miod Exp $	*/

#ifndef _M88K_SPINLOCK_H_
#define _M88K_SPINLOCK_H_

#define _ATOMIC_LOCK_UNLOCKED	(0)
#define _ATOMIC_LOCK_LOCKED	(1)
typedef int _atomic_lock_t;

#ifndef _KERNEL
int _atomic_lock(volatile _atomic_lock_t *);
#endif

#endif
