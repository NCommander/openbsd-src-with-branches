/*	$OpenBSD: spinlock.h,v 1.3 2011/11/14 14:29:53 deraadt Exp $	*/

#ifndef _MACHINE_SPINLOCK_H_
#define _MACHINE_SPINLOCK_H_

#define _ATOMIC_LOCK_UNLOCKED	(1)
#define _ATOMIC_LOCK_LOCKED	(0)
typedef long _atomic_lock_t __attribute__((__aligned__(16)));

#ifndef _KERNEL
int _atomic_lock(volatile _atomic_lock_t *);
#endif

#endif
