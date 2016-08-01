/*	$OpenBSD: spinlock.h,v 1.1 2001/09/10 20:00:14 jason Exp $	*/

#ifndef _MACHINE_SPINLOCK_H_
#define _MACHINE_SPINLOCK_H_

#define _ATOMIC_LOCK_UNLOCKED	(0x00)
#define _ATOMIC_LOCK_LOCKED	(0xFF)
typedef unsigned char _atomic_lock_t;

#ifndef _KERNEL
int _atomic_lock(volatile _atomic_lock_t *);
#endif

#endif
