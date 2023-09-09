/* $OpenBSD: kern_clockintr.c,v 1.44 2023/09/09 16:59:01 cheloha Exp $ */
/*
 * Copyright (c) 2003 Dale Rahn <drahn@openbsd.org>
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
 * Copyright (c) 2020-2022 Scott Cheloha <cheloha@openbsd.org>
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
#include <sys/clockintr.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/resourcevar.h>
#include <sys/queue.h>
#include <sys/sched.h>
#include <sys/stdint.h>
#include <sys/sysctl.h>
#include <sys/time.h>

/*
 * Protection for global variables in this file:
 *
 *	I	Immutable after initialization.
 */
uint32_t clockintr_flags;		/* [I] global state + behavior flags */
uint32_t hardclock_period;		/* [I] hardclock period (ns) */
uint32_t statclock_avg;			/* [I] average statclock period (ns) */
uint32_t statclock_min;			/* [I] minimum statclock period (ns) */
uint32_t statclock_mask;		/* [I] set of allowed offsets */

uint64_t clockintr_advance_random(struct clockintr *, uint64_t, uint32_t);
void clockintr_hardclock(struct clockintr *, void *);
void clockintr_schedule(struct clockintr *, uint64_t);
void clockintr_schedule_locked(struct clockintr *, uint64_t);
void clockintr_statclock(struct clockintr *, void *);
void clockqueue_intrclock_install(struct clockintr_queue *,
    const struct intrclock *);
uint64_t clockqueue_next(const struct clockintr_queue *);
void clockqueue_pend_delete(struct clockintr_queue *, struct clockintr *);
void clockqueue_pend_insert(struct clockintr_queue *, struct clockintr *,
    uint64_t);
void clockqueue_reset_intrclock(struct clockintr_queue *);
uint64_t nsec_advance(uint64_t *, uint64_t, uint64_t);

/*
 * Initialize global state.  Set flags and compute intervals.
 */
void
clockintr_init(uint32_t flags)
{
	uint32_t half_avg, var;

	KASSERT(CPU_IS_PRIMARY(curcpu()));
	KASSERT(clockintr_flags == 0);
	KASSERT(!ISSET(flags, ~CL_FLAG_MASK));

	KASSERT(hz > 0 && hz <= 1000000000);
	hardclock_period = 1000000000 / hz;
	roundrobin_period = hardclock_period * 10;

	KASSERT(stathz >= 1 && stathz <= 1000000000);

	/*
	 * Compute the average statclock() period.  Then find var, the
	 * largest power of two such that var <= statclock_avg / 2.
	 */
	statclock_avg = 1000000000 / stathz;
	half_avg = statclock_avg / 2;
	for (var = 1U << 31; var > half_avg; var /= 2)
		continue;

	/*
	 * Set a lower bound for the range using statclock_avg and var.
	 * The mask for that range is just (var - 1).
	 */
	statclock_min = statclock_avg - (var / 2);
	statclock_mask = var - 1;

	SET(clockintr_flags, flags | CL_INIT);
}

/*
 * Ready the calling CPU for clockintr_dispatch().  If this is our
 * first time here, install the intrclock, if any, and set necessary
 * flags.  Advance the schedule as needed.
 */
void
clockintr_cpu_init(const struct intrclock *ic)
{
	uint64_t multiplier = 0;
	struct cpu_info *ci = curcpu();
	struct clockintr_queue *cq = &ci->ci_queue;
	struct schedstate_percpu *spc = &ci->ci_schedstate;
	int reset_cq_intrclock = 0;

	KASSERT(ISSET(clockintr_flags, CL_INIT));

	if (ic != NULL)
		clockqueue_intrclock_install(cq, ic);

	/* TODO: Remove these from struct clockintr_queue. */
	if (cq->cq_hardclock == NULL) {
		cq->cq_hardclock = clockintr_establish(ci, clockintr_hardclock);
		if (cq->cq_hardclock == NULL)
			panic("%s: failed to establish hardclock", __func__);
	}
	if (cq->cq_statclock == NULL) {
		cq->cq_statclock = clockintr_establish(ci, clockintr_statclock);
		if (cq->cq_statclock == NULL)
			panic("%s: failed to establish statclock", __func__);
	}

	/*
	 * Mask CQ_INTRCLOCK while we're advancing the internal clock
	 * interrupts.  We don't want the intrclock to fire until this
	 * thread reaches clockintr_trigger().
	 */
	if (ISSET(cq->cq_flags, CQ_INTRCLOCK)) {
		CLR(cq->cq_flags, CQ_INTRCLOCK);
		reset_cq_intrclock = 1;
	}

	/*
	 * Until we understand scheduler lock contention better, stagger
	 * the hardclock and statclock so they don't all happen at once.
	 * If we have no intrclock it doesn't matter, we have no control
	 * anyway.  The primary CPU's starting offset is always zero, so
	 * leave the multiplier zero.
	 */
	if (!CPU_IS_PRIMARY(ci) && reset_cq_intrclock)
		multiplier = CPU_INFO_UNIT(ci);

	/*
	 * The first time we do this, the primary CPU cannot skip any
	 * hardclocks.  We can skip hardclocks on subsequent calls because
	 * the global tick value is advanced during inittodr(9) on our
	 * behalf.
	 */
	if (CPU_IS_PRIMARY(ci)) {
		if (cq->cq_hardclock->cl_expiration == 0)
			clockintr_schedule(cq->cq_hardclock, 0);
		else
			clockintr_advance(cq->cq_hardclock, hardclock_period);
	} else {
		if (cq->cq_hardclock->cl_expiration == 0) {
			clockintr_stagger(cq->cq_hardclock, hardclock_period,
			     multiplier, MAXCPUS);
		}
		clockintr_advance(cq->cq_hardclock, hardclock_period);
	}

	/*
	 * We can always advance the statclock.  There is no reason to
	 * stagger a randomized statclock.
	 */
	if (!ISSET(clockintr_flags, CL_RNDSTAT)) {
		if (cq->cq_statclock->cl_expiration == 0) {
			clockintr_stagger(cq->cq_statclock, statclock_avg,
			    multiplier, MAXCPUS);
		}
	}
	clockintr_advance(cq->cq_statclock, statclock_avg);

	/*
	 * XXX Need to find a better place to do this.  We can't do it in
	 * sched_init_cpu() because initclocks() runs after it.
	 */
	if (spc->spc_itimer->cl_expiration == 0) {
		clockintr_stagger(spc->spc_itimer, hardclock_period,
		    multiplier, MAXCPUS);
	}
	if (spc->spc_profclock->cl_expiration == 0) {
		clockintr_stagger(spc->spc_profclock, profclock_period,
		    multiplier, MAXCPUS);
	}
	if (spc->spc_roundrobin->cl_expiration == 0) {
		clockintr_stagger(spc->spc_roundrobin, hardclock_period,
		    multiplier, MAXCPUS);
	}
	clockintr_advance(spc->spc_roundrobin, roundrobin_period);

	if (reset_cq_intrclock)
		SET(cq->cq_flags, CQ_INTRCLOCK);
}

/*
 * If we have an intrclock, trigger it to start the dispatch cycle.
 */
void
clockintr_trigger(void)
{
	struct clockintr_queue *cq = &curcpu()->ci_queue;

	KASSERT(ISSET(cq->cq_flags, CQ_INIT));

	if (ISSET(cq->cq_flags, CQ_INTRCLOCK))
		intrclock_trigger(&cq->cq_intrclock);
}

/*
 * Run all expired events scheduled on the calling CPU.
 */
int
clockintr_dispatch(void *frame)
{
	uint64_t lateness, run = 0, start;
	struct cpu_info *ci = curcpu();
	struct clockintr *cl;
	struct clockintr_queue *cq = &ci->ci_queue;
	uint32_t ogen;

	if (cq->cq_dispatch != 0)
		panic("%s: recursive dispatch", __func__);
	cq->cq_dispatch = 1;

	splassert(IPL_CLOCK);
	KASSERT(ISSET(cq->cq_flags, CQ_INIT));

	mtx_enter(&cq->cq_mtx);

	/*
	 * If nothing is scheduled or we arrived too early, we have
	 * nothing to do.
	 */
	start = nsecuptime();
	cq->cq_uptime = start;
	if (TAILQ_EMPTY(&cq->cq_pend))
		goto stats;
	if (cq->cq_uptime < clockqueue_next(cq))
		goto rearm;
	lateness = start - clockqueue_next(cq);

	/*
	 * Dispatch expired events.
	 */
	for (;;) {
		cl = TAILQ_FIRST(&cq->cq_pend);
		if (cl == NULL)
			break;
		if (cq->cq_uptime < cl->cl_expiration) {
			/* Double-check the time before giving up. */
			cq->cq_uptime = nsecuptime();
			if (cq->cq_uptime < cl->cl_expiration)
				break;
		}
		clockqueue_pend_delete(cq, cl);
		cq->cq_shadow.cl_expiration = cl->cl_expiration;
		cq->cq_shadow.cl_func = cl->cl_func;
		cq->cq_running = cl;
		mtx_leave(&cq->cq_mtx);

		cq->cq_shadow.cl_func(&cq->cq_shadow, frame);

		mtx_enter(&cq->cq_mtx);
		cq->cq_running = NULL;
		if (ISSET(cl->cl_flags, CLST_IGNORE_SHADOW)) {
			CLR(cl->cl_flags, CLST_IGNORE_SHADOW);
			CLR(cq->cq_shadow.cl_flags, CLST_SHADOW_PENDING);
		}
		if (ISSET(cq->cq_shadow.cl_flags, CLST_SHADOW_PENDING)) {
			CLR(cq->cq_shadow.cl_flags, CLST_SHADOW_PENDING);
			clockqueue_pend_insert(cq, cl,
			    cq->cq_shadow.cl_expiration);
		}
		run++;
	}

	/*
	 * Dispatch complete.
	 */
rearm:
	/* Rearm the interrupt clock if we have one. */
	if (ISSET(cq->cq_flags, CQ_INTRCLOCK)) {
		if (!TAILQ_EMPTY(&cq->cq_pend)) {
			intrclock_rearm(&cq->cq_intrclock,
			    clockqueue_next(cq) - cq->cq_uptime);
		}
	}
stats:
	/* Update our stats. */
	ogen = cq->cq_gen;
	cq->cq_gen = 0;
	membar_producer();
	cq->cq_stat.cs_dispatched += cq->cq_uptime - start;
	if (run > 0) {
		cq->cq_stat.cs_lateness += lateness;
		cq->cq_stat.cs_prompt++;
		cq->cq_stat.cs_run += run;
	} else if (!TAILQ_EMPTY(&cq->cq_pend)) {
		cq->cq_stat.cs_early++;
		cq->cq_stat.cs_earliness += clockqueue_next(cq) - cq->cq_uptime;
	} else
		cq->cq_stat.cs_spurious++;
	membar_producer();
	cq->cq_gen = MAX(1, ogen + 1);

	mtx_leave(&cq->cq_mtx);

	if (cq->cq_dispatch != 1)
		panic("%s: unexpected value: %u", __func__, cq->cq_dispatch);
	cq->cq_dispatch = 0;

	return run > 0;
}

uint64_t
clockintr_advance(struct clockintr *cl, uint64_t period)
{
	uint64_t count, expiration;
	struct clockintr_queue *cq = cl->cl_queue;

	if (cl == &cq->cq_shadow) {
		count = nsec_advance(&cl->cl_expiration, period, cq->cq_uptime);
		SET(cl->cl_flags, CLST_SHADOW_PENDING);
	} else {
		mtx_enter(&cq->cq_mtx);
		expiration = cl->cl_expiration;
		count = nsec_advance(&expiration, period, nsecuptime());
		clockintr_schedule_locked(cl, expiration);
		mtx_leave(&cq->cq_mtx);
	}
	return count;
}

uint64_t
clockintr_advance_random(struct clockintr *cl, uint64_t min, uint32_t mask)
{
	uint64_t count = 0;
	struct clockintr_queue *cq = cl->cl_queue;
	uint32_t off;

	KASSERT(cl == &cq->cq_shadow);

	while (cl->cl_expiration <= cq->cq_uptime) {
		while ((off = (random() & mask)) == 0)
			continue;
		cl->cl_expiration += min + off;
		count++;
	}
	SET(cl->cl_flags, CLST_SHADOW_PENDING);
	return count;
}

void
clockintr_cancel(struct clockintr *cl)
{
	struct clockintr_queue *cq = cl->cl_queue;
	int was_next;

	if (cl == &cq->cq_shadow) {
		CLR(cl->cl_flags, CLST_SHADOW_PENDING);
		return;
	}

	mtx_enter(&cq->cq_mtx);
	if (ISSET(cl->cl_flags, CLST_PENDING)) {
		was_next = cl == TAILQ_FIRST(&cq->cq_pend);
		clockqueue_pend_delete(cq, cl);
		if (ISSET(cq->cq_flags, CQ_INTRCLOCK)) {
			if (was_next && !TAILQ_EMPTY(&cq->cq_pend)) {
				if (cq == &curcpu()->ci_queue)
					clockqueue_reset_intrclock(cq);
			}
		}
	}
	if (cl == cq->cq_running)
		SET(cl->cl_flags, CLST_IGNORE_SHADOW);
	mtx_leave(&cq->cq_mtx);
}

struct clockintr *
clockintr_establish(struct cpu_info *ci,
    void (*func)(struct clockintr *, void *))
{
	struct clockintr *cl;
	struct clockintr_queue *cq = &ci->ci_queue;

	cl = malloc(sizeof *cl, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (cl == NULL)
		return NULL;
	cl->cl_func = func;
	cl->cl_queue = cq;

	mtx_enter(&cq->cq_mtx);
	TAILQ_INSERT_TAIL(&cq->cq_est, cl, cl_elink);
	mtx_leave(&cq->cq_mtx);
	return cl;
}

void
clockintr_schedule(struct clockintr *cl, uint64_t expiration)
{
	struct clockintr_queue *cq = cl->cl_queue;

	if (cl == &cq->cq_shadow) {
		cl->cl_expiration = expiration;
		SET(cl->cl_flags, CLST_SHADOW_PENDING);
	} else {
		mtx_enter(&cq->cq_mtx);
		clockintr_schedule_locked(cl, expiration);
		mtx_leave(&cq->cq_mtx);
	}
}

void
clockintr_schedule_locked(struct clockintr *cl, uint64_t expiration)
{
	struct clockintr_queue *cq = cl->cl_queue;

	MUTEX_ASSERT_LOCKED(&cq->cq_mtx);

	if (ISSET(cl->cl_flags, CLST_PENDING))
		clockqueue_pend_delete(cq, cl);
	clockqueue_pend_insert(cq, cl, expiration);
	if (ISSET(cq->cq_flags, CQ_INTRCLOCK)) {
		if (cl == TAILQ_FIRST(&cq->cq_pend)) {
			if (cq == &curcpu()->ci_queue)
				clockqueue_reset_intrclock(cq);
		}
	}
	if (cl == cq->cq_running)
		SET(cl->cl_flags, CLST_IGNORE_SHADOW);
}

void
clockintr_stagger(struct clockintr *cl, uint64_t period, uint32_t n,
    uint32_t count)
{
	struct clockintr_queue *cq = cl->cl_queue;

	KASSERT(n < count);

	mtx_enter(&cq->cq_mtx);
	if (ISSET(cl->cl_flags, CLST_PENDING))
		panic("%s: clock interrupt pending", __func__);
	cl->cl_expiration = period / count * n;
	mtx_leave(&cq->cq_mtx);
}

void
clockintr_hardclock(struct clockintr *cl, void *frame)
{
	uint64_t count, i;

	count = clockintr_advance(cl, hardclock_period);
	for (i = 0; i < count; i++)
		hardclock(frame);
}

void
clockintr_statclock(struct clockintr *cl, void *frame)
{
	uint64_t count, i;

	if (ISSET(clockintr_flags, CL_RNDSTAT)) {
		count = clockintr_advance_random(cl, statclock_min,
		    statclock_mask);
	} else {
		count = clockintr_advance(cl, statclock_avg);
	}
	for (i = 0; i < count; i++)
		statclock(frame);
}

void
clockqueue_init(struct clockintr_queue *cq)
{
	if (ISSET(cq->cq_flags, CQ_INIT))
		return;

	cq->cq_shadow.cl_queue = cq;
	mtx_init(&cq->cq_mtx, IPL_CLOCK);
	TAILQ_INIT(&cq->cq_est);
	TAILQ_INIT(&cq->cq_pend);
	cq->cq_gen = 1;
	SET(cq->cq_flags, CQ_INIT);
}

void
clockqueue_intrclock_install(struct clockintr_queue *cq,
    const struct intrclock *ic)
{
	mtx_enter(&cq->cq_mtx);
	if (!ISSET(cq->cq_flags, CQ_INTRCLOCK)) {
		cq->cq_intrclock = *ic;
		SET(cq->cq_flags, CQ_INTRCLOCK);
	}
	mtx_leave(&cq->cq_mtx);
}

uint64_t
clockqueue_next(const struct clockintr_queue *cq)
{
	MUTEX_ASSERT_LOCKED(&cq->cq_mtx);
	return TAILQ_FIRST(&cq->cq_pend)->cl_expiration;
}

void
clockqueue_pend_delete(struct clockintr_queue *cq, struct clockintr *cl)
{
	MUTEX_ASSERT_LOCKED(&cq->cq_mtx);
	KASSERT(ISSET(cl->cl_flags, CLST_PENDING));

	TAILQ_REMOVE(&cq->cq_pend, cl, cl_plink);
	CLR(cl->cl_flags, CLST_PENDING);
}


void
clockqueue_pend_insert(struct clockintr_queue *cq, struct clockintr *cl,
    uint64_t expiration)
{
	struct clockintr *elm;

	MUTEX_ASSERT_LOCKED(&cq->cq_mtx);
	KASSERT(!ISSET(cl->cl_flags, CLST_PENDING));

	cl->cl_expiration = expiration;
	TAILQ_FOREACH(elm, &cq->cq_pend, cl_plink) {
		if (cl->cl_expiration < elm->cl_expiration)
			break;
	}
	if (elm == NULL)
		TAILQ_INSERT_TAIL(&cq->cq_pend, cl, cl_plink);
	else
		TAILQ_INSERT_BEFORE(elm, cl, cl_plink);
	SET(cl->cl_flags, CLST_PENDING);
}

void
clockqueue_reset_intrclock(struct clockintr_queue *cq)
{
	uint64_t exp, now;

	MUTEX_ASSERT_LOCKED(&cq->cq_mtx);
	KASSERT(ISSET(cq->cq_flags, CQ_INTRCLOCK));

	exp = clockqueue_next(cq);
	now = nsecuptime();
	if (now < exp)
		intrclock_rearm(&cq->cq_intrclock, exp - now);
	else
		intrclock_trigger(&cq->cq_intrclock);
}

/*
 * Advance *next in increments of period until it exceeds now.
 * Returns the number of increments *next was advanced.
 *
 * We check the common cases first to avoid division if possible.
 * This does no overflow checking.
 */
uint64_t
nsec_advance(uint64_t *next, uint64_t period, uint64_t now)
{
	uint64_t elapsed;

	if (now < *next)
		return 0;

	if (now < *next + period) {
		*next += period;
		return 1;
	}

	elapsed = (now - *next) / period + 1;
	*next += period * elapsed;
	return elapsed;
}

int
sysctl_clockintr(int *name, u_int namelen, void *oldp, size_t *oldlenp,
    void *newp, size_t newlen)
{
	struct clockintr_stat sum, tmp;
	struct clockintr_queue *cq;
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;
	uint32_t gen;

	if (namelen != 1)
		return ENOTDIR;

	switch (name[0]) {
	case KERN_CLOCKINTR_STATS:
		memset(&sum, 0, sizeof sum);
		CPU_INFO_FOREACH(cii, ci) {
			cq = &ci->ci_queue;
			if (!ISSET(cq->cq_flags, CQ_INIT))
				continue;
			do {
				gen = cq->cq_gen;
				membar_consumer();
				tmp = cq->cq_stat;
				membar_consumer();
			} while (gen == 0 || gen != cq->cq_gen);
			sum.cs_dispatched += tmp.cs_dispatched;
			sum.cs_early += tmp.cs_early;
			sum.cs_earliness += tmp.cs_earliness;
			sum.cs_lateness += tmp.cs_lateness;
			sum.cs_prompt += tmp.cs_prompt;
			sum.cs_run += tmp.cs_run;
			sum.cs_spurious += tmp.cs_spurious;
		}
		return sysctl_rdstruct(oldp, oldlenp, newp, &sum, sizeof sum);
	default:
		break;
	}

	return EINVAL;
}

#ifdef DDB

#include <machine/db_machdep.h>

#include <ddb/db_interface.h>
#include <ddb/db_output.h>
#include <ddb/db_sym.h>

void db_show_clockintr(const struct clockintr *, const char *, u_int);
void db_show_clockintr_cpu(struct cpu_info *);

void
db_show_all_clockintr(db_expr_t addr, int haddr, db_expr_t count, char *modif)
{
	struct timespec now;
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;

	nanouptime(&now);
	db_printf("%20s\n", "UPTIME");
	db_printf("%10lld.%09ld\n", now.tv_sec, now.tv_nsec);
	db_printf("\n");
	db_printf("%20s  %5s  %3s  %s\n", "EXPIRATION", "STATE", "CPU", "NAME");
	CPU_INFO_FOREACH(cii, ci) {
		if (ISSET(ci->ci_queue.cq_flags, CQ_INIT))
			db_show_clockintr_cpu(ci);
	}
}

void
db_show_clockintr_cpu(struct cpu_info *ci)
{
	struct clockintr *elm;
	struct clockintr_queue *cq = &ci->ci_queue;
	u_int cpu = CPU_INFO_UNIT(ci);

	if (cq->cq_running != NULL)
		db_show_clockintr(cq->cq_running, "run", cpu);
	TAILQ_FOREACH(elm, &cq->cq_pend, cl_plink)
		db_show_clockintr(elm, "pend", cpu);
	TAILQ_FOREACH(elm, &cq->cq_est, cl_elink) {
		if (!ISSET(elm->cl_flags, CLST_PENDING))
			db_show_clockintr(elm, "idle", cpu);
	}
}

void
db_show_clockintr(const struct clockintr *cl, const char *state, u_int cpu)
{
	struct timespec ts;
	char *name;
	db_expr_t offset;

	NSEC_TO_TIMESPEC(cl->cl_expiration, &ts);
	db_find_sym_and_offset((vaddr_t)cl->cl_func, &name, &offset);
	if (name == NULL)
		name = "?";
	db_printf("%10lld.%09ld  %5s  %3u  %s\n",
	    ts.tv_sec, ts.tv_nsec, state, cpu, name);
}

#endif /* DDB */
