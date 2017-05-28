/*	$OpenBSD: lock_machdep.c,v 1.14 2017/04/30 16:45:45 mpi Exp $	*/

/*
 * Copyright (c) 2007 Artur Grabowski <art@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/atomic.h>
#include <sys/witness.h>
#include <sys/_lock.h>

#include <machine/lock.h>
#include <machine/cpufunc.h>

#include <ddb/db_output.h>

void
___mp_lock_init(struct __mp_lock *mpl)
{
	memset(mpl->mpl_cpus, 0, sizeof(mpl->mpl_cpus));
	mpl->mpl_users = 0;
	mpl->mpl_ticket = 1;
}

#if defined(MP_LOCKDEBUG)
#ifndef DDB
#error "MP_LOCKDEBUG requires DDB"
#endif

/* CPU-dependent timing, needs this to be settable from ddb. */
extern int __mp_lock_spinout;
#endif

static __inline void
__mp_lock_spin(struct __mp_lock *mpl, u_int me)
{
#ifndef MP_LOCKDEBUG
	while (mpl->mpl_ticket != me)
		SPINLOCK_SPIN_HOOK;
#else
	int nticks = __mp_lock_spinout;

	while (mpl->mpl_ticket != me) {
		SPINLOCK_SPIN_HOOK;

		if (--nticks <= 0) {
			db_printf("__mp_lock(%p): lock spun out", mpl);
			db_enter();
			nticks = __mp_lock_spinout;
		}
	}
#endif
}

void
___mp_lock(struct __mp_lock *mpl LOCK_FL_VARS)
{
	struct __mp_lock_cpu *cpu = &mpl->mpl_cpus[cpu_number()];
	unsigned long s;
#ifdef WITNESS
	int lock_held;

	lock_held = __mp_lock_held(mpl);
	if (!lock_held)
		WITNESS_CHECKORDER(&mpl->mpl_lock_obj,
		    LOP_EXCLUSIVE | LOP_NEWORDER, file, line, NULL);
#endif

	s = intr_disable();
	if (cpu->mplc_depth++ == 0)
		cpu->mplc_ticket = atomic_inc_int_nv(&mpl->mpl_users);
	intr_restore(s);

	__mp_lock_spin(mpl, cpu->mplc_ticket);

	WITNESS_LOCK(&mpl->mpl_lock_obj, LOP_EXCLUSIVE, file, line);
}

void
___mp_unlock(struct __mp_lock *mpl LOCK_FL_VARS)
{
	struct __mp_lock_cpu *cpu = &mpl->mpl_cpus[cpu_number()];
	unsigned long s;

#ifdef MP_LOCKDEBUG
	if (!__mp_lock_held(mpl)) {
		db_printf("__mp_unlock(%p): not held lock\n", mpl);
		db_enter();
	}
#endif

	WITNESS_UNLOCK(&mpl->mpl_lock_obj, LOP_EXCLUSIVE, file, line);

	s = intr_disable();
	if (--cpu->mplc_depth == 0)
		mpl->mpl_ticket++;
	intr_restore(s);
}

int
___mp_release_all(struct __mp_lock *mpl LOCK_FL_VARS)
{
	struct __mp_lock_cpu *cpu = &mpl->mpl_cpus[cpu_number()];
	unsigned long s;
	int rv;
#ifdef WITNESS
	int i;
#endif

	s = intr_disable();
 	rv = cpu->mplc_depth;
#ifdef WITNESS
	for (i = 0; i < rv; i++)
		WITNESS_UNLOCK(&mpl->mpl_lock_obj, LOP_EXCLUSIVE, file, line);
#endif
	cpu->mplc_depth = 0;
	mpl->mpl_ticket++;
	intr_restore(s);

	return (rv);
}

int
___mp_release_all_but_one(struct __mp_lock *mpl LOCK_FL_VARS)
{
	struct __mp_lock_cpu *cpu = &mpl->mpl_cpus[cpu_number()];
	int rv = cpu->mplc_depth - 1;
#ifdef WITNESS
	int i;

	for (i = 0; i < rv; i++)
		WITNESS_UNLOCK(&mpl->mpl_lock_obj, LOP_EXCLUSIVE, file, line);
#endif

#ifdef MP_LOCKDEBUG
	if (!__mp_lock_held(mpl)) {
		db_printf("__mp_release_all_but_one(%p): not held lock\n", mpl);
		db_enter();
	}
#endif

	cpu->mplc_depth = 1;

	return (rv);
}

void
___mp_acquire_count(struct __mp_lock *mpl, int count LOCK_FL_VARS)
{
	while (count--)
		___mp_lock(mpl LOCK_FL_ARGS);
}

int
__mp_lock_held(struct __mp_lock *mpl)
{
	struct __mp_lock_cpu *cpu = &mpl->mpl_cpus[cpu_number()];

	return (cpu->mplc_ticket == mpl->mpl_ticket && cpu->mplc_depth > 0);
}
