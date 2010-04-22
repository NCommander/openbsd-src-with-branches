/*	$OpenBSD: atomic.h,v 1.6 2010/04/21 03:03:25 deraadt Exp $	*/

/* Public Domain */

#ifndef __ARM_ATOMIC_H__
#define __ARM_ATOMIC_H__

#if defined(_KERNEL)

/*
 * on pre-v6 arm processors, it is necessary to disable interrupts if
 * in the kernel and atomic updates are necessary without full mutexes
 */

void atomic_setbits_int(__volatile unsigned int *, unsigned int);
void atomic_clearbits_int(__volatile unsigned int *, unsigned int);

#endif /* defined(_KERNEL) */
#endif /* __ARM_ATOMIC_H__ */
