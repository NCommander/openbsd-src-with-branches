/*	$OpenBSD: spinlock.h,v 1.2 1999/01/26 23:39:28 d Exp $	*/

#ifndef _M68K_SPINLOCK_H_
#define _M68K_SPINLOCK_H_

#define _ATOMIC_LOCK_UNLOCKED	(0)
#define _ATOMIC_LOCK_LOCKED	(1)
typedef int _atomic_lock_t;

#ifndef _KERNEL
int _atomic_lock(volatile _atomic_lock_t *);
#endif

#endif
