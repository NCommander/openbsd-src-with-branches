/*	$OpenBSD: mplock.h,v 1.3 2017/12/04 09:51:03 mpi Exp $	*/

/*
 * Copyright (c) 2004 Niklas Hallqvist.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _POWERPC_MPLOCK_H_
#define _POWERPC_MPLOCK_H_

#define __USE_MI_MPLOCK

/*
 * Really simple spinlock implementation with recursive capabilities.
 * Correctness is paramount, no fancyness allowed.
 */

struct __ppc_lock {
	volatile struct cpu_info *mpl_cpu;
	volatile long		mpl_count;
};

#ifndef _LOCORE

void __ppc_lock_init(struct __ppc_lock *);
void __ppc_lock(struct __ppc_lock *);
void __ppc_unlock(struct __ppc_lock *);
int __ppc_release_all(struct __ppc_lock *);
int __ppc_release_all_but_one(struct __ppc_lock *);
void __ppc_acquire_count(struct __ppc_lock *, int);
int __ppc_lock_held(struct __ppc_lock *, struct cpu_info *);

#endif

#endif /* !_POWERPC_MPLOCK_H */
