/*	$OpenBSD: spinlock.h,v 1.1 2004/01/20 04:24:56 drahn Exp $	*/

#ifndef _ARM_SPINLOCK_H_
#define _ARM_SPINLOCK_H_

#define _SPINLOCK_UNLOCKED	(0)
#define _SPINLOCK_LOCKED	(1)
typedef int _spinlock_lock_t;

#endif
