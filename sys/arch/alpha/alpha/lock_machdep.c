/*	$OpenBSD: lock_machdep.c,v 1.7 2017/12/04 09:51:03 mpi Exp $	*/

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

#include <machine/atomic.h>
#include <machine/cpu.h>

#if defined(MP_LOCKDEBUG)
#ifndef DDB
#error "MP_LOCKDEBUG requires DDB"
#endif
#include <ddb/db_output.h>

/* CPU-dependent timing, needs this to be settable from ddb. */
extern int __mp_lock_spinout;
#endif

static inline int
__cpu_cas(volatile unsigned long *addr, unsigned long old, unsigned long new)
{
	unsigned long t0, v0;

	__asm volatile(
		"1:	ldq_l	%1, 0(%2)	\n"	/* v0 = *addr */
		"	cmpeq	%1, %3, %0	\n"	/* t0 = v0 == old */
		"	beq	%0, 2f		\n"
		"	mov	%4, %0		\n"	/* t0 = new */
		"	stq_c	%0, 0(%2)	\n"	/* *addr = new */
		"	beq	%0, 3f		\n"
		"	mb			\n"
		"2:	br	4f		\n"
		"3:	br	1b		\n"	/* update failed */
		"4:				\n"
		: "=&r" (t0), "=&r" (v0)
		: "r" (addr), "r" (old), "r" (new)
		: "memory");

	return (v0 != old);
}

void
__mp_lock_init(struct __mp_lock *lock)
{
	lock->mpl_cpu = NULL;
	lock->mpl_count = 0;
}

static inline void
__mp_lock_spin(struct __mp_lock *mpl)
{
#ifndef MP_LOCKDEBUG
	while (mpl->mpl_count != 0)
		CPU_BUSY_CYCLE();
#else
	int nticks = __mp_lock_spinout;
	if (!CPU_IS_PRIMARY(curcpu()))
		nticks += nticks;

	while (mpl->mpl_count != 0 && --nticks > 0)
		CPU_BUSY_CYCLE();

	if (nticks == 0) {
		db_printf("__mp_lock(%p): lock spun out\n", mpl);
		db_enter();
	}
#endif
}

void
__mp_lock(struct __mp_lock *mpl)
{
	int s;
	struct cpu_info *ci = curcpu();

	/*
	 * Please notice that mpl_count gets incremented twice for the
	 * first lock. This is on purpose. The way we release the lock
	 * in mp_unlock is to decrement the mpl_count and then check if
	 * the lock should be released. Since mpl_count is what we're
	 * spinning on, decrementing it in mpl_unlock to 0 means that
	 * we can't clear mpl_cpu, because we're no longer holding the
	 * lock. In theory mpl_cpu doesn't need to be cleared, but it's
	 * safer to clear it and besides, setting mpl_count to 2 on the
	 * first lock makes most of this code much simpler.
	 */
	while (1) {
		s = splhigh();
		if (__cpu_cas(&mpl->mpl_count, 0, 1) == 0) {
			alpha_mb();
			mpl->mpl_cpu = ci;
		}

		if (mpl->mpl_cpu == ci) {
			mpl->mpl_count++;
			splx(s);
			break;
		}
		splx(s);
		
		__mp_lock_spin(mpl);
	}
}

void
__mp_unlock(struct __mp_lock *mpl)
{
	int s;

#ifdef MP_LOCKDEBUG
	if (mpl->mpl_cpu != curcpu()) {
		db_printf("__mp_unlock(%p): not held lock\n", mpl);
		db_enter();
	}
#endif

	s = splhigh();
	if (--mpl->mpl_count == 1) {
		mpl->mpl_cpu = NULL;
		alpha_mb();
		mpl->mpl_count = 0;
	}
	splx(s);
}

int
__mp_release_all(struct __mp_lock *mpl)
{
	int rv = mpl->mpl_count - 1;
	int s;

#ifdef MP_LOCKDEBUG
	if (mpl->mpl_cpu != curcpu()) {
		db_printf("__mp_release_all(%p): not held lock\n", mpl);
		db_enter();
	}
#endif

	s = splhigh();
	mpl->mpl_cpu = NULL;
	alpha_mb();
	mpl->mpl_count = 0;
	splx(s);

	return (rv);
}

int
__mp_release_all_but_one(struct __mp_lock *mpl)
{
	int rv = mpl->mpl_count - 2;
#ifdef MP_LOCKDEBUG
	if (mpl->mpl_cpu != curcpu()) {
		db_printf("__mp_release_all_but_one(%p): not held lock\n", mpl);
		db_enter();
	}
#endif

	mpl->mpl_count = 2;

	return (rv);
}

void
__mp_acquire_count(struct __mp_lock *mpl, int count)
{
	while (count--)
		__mp_lock(mpl);
}

int
__mp_lock_held(struct __mp_lock *mpl, struct cpu_info *ci)
{
	return (mpl->mpl_cpu == ci);
}
