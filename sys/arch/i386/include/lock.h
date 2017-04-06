/*	$OpenBSD: lock.h,v 1.11 2015/05/30 08:41:30 kettenis Exp $	*/

/* public domain */

#ifndef _MACHINE_LOCK_H_
#define	_MACHINE_LOCK_H_

#define SPINLOCK_SPIN_HOOK __asm volatile("pause": : :"memory")

#endif /* _MACHINE_LOCK_H_ */
