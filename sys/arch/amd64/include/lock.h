/*	$OpenBSD: lock.h,v 1.10 2015/02/11 00:14:11 dlg Exp $	*/

/* public domain */

#ifndef _MACHINE_LOCK_H_
#define	_MACHINE_LOCK_H_

#define SPINLOCK_SPIN_HOOK __asm volatile("pause": : :"memory");

#endif /* _MACHINE_LOCK_H_ */
