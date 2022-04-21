/*	$OpenBSD: spinlock.h,v 1.3 2013/06/01 20:47:40 tedu Exp $	*/

#ifndef _POWERPC_SPINLOCK_H_
#define _POWERPC_SPINLOCK_H_

#define _ATOMIC_LOCK_UNLOCKED	(0)
#define _ATOMIC_LOCK_LOCKED	(1)
typedef int _atomic_lock_t;

#endif
