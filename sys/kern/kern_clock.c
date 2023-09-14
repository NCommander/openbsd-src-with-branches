/*	$OpenBSD: kern_clock.c,v 1.116 2023/09/09 18:19:03 cheloha Exp $	*/
/*	$NetBSD: kern_clock.c,v 1.34 1996/06/09 04:51:03 briggs Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)kern_clock.c	8.5 (Berkeley) 1/21/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/timeout.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/resourcevar.h>
#include <sys/sysctl.h>
#include <sys/sched.h>
#include <sys/timetc.h>

#include "dt.h"
#if NDT > 0
#include <dev/dt/dtvar.h>
#endif

/*
 * Clock handling routines.
 *
 * This code is written to operate with two timers that run independently of
 * each other.  The main clock, running hz times per second, is used to keep
 * track of real time.  The second timer handles kernel and user profiling,
 * and does resource use estimation.  If the second timer is programmable,
 * it is randomized to avoid aliasing between the two clocks.  For example,
 * the randomization prevents an adversary from always giving up the cpu
 * just before its quantum expires.  Otherwise, it would never accumulate
 * cpu ticks.  The mean frequency of the second timer is stathz.
 *
 * If no second timer exists, stathz will be zero; in this case we drive
 * profiling and statistics off the main clock.  This WILL NOT be accurate;
 * do not do it unless absolutely necessary.
 *
 * The statistics clock may (or may not) be run at a higher rate while
 * profiling.  This profile clock runs at profhz.  We require that profhz
 * be an integral multiple of stathz.
 *
 * If the statistics clock is running fast, it must be divided by the ratio
 * profhz/stathz for statistics.  (For profiling, every tick counts.)
 */

int	stathz;
int	profhz;
int	profprocs;
int	ticks = INT_MAX - (15 * 60 * HZ);

/* Don't force early wrap around, triggers bug in inteldrm */
volatile unsigned long jiffies;

int statclock_is_randomized;	/* [I] fixed or pseudorandom period? */

/*
 * Initialize clock frequencies and start both clocks running.
 */
void
initclocks(void)
{
	/*
	 * Let the machine-specific code do its bit.
	 */
	cpu_initclocks();

	KASSERT(profhz >= stathz && profhz <= 1000000000);
	KASSERT(profhz % stathz == 0);
	profclock_period = 1000000000 / profhz;

	inittimecounter();

	/* Start dispatching clock interrupts on the primary CPU. */
	cpu_startclock();
}

/*
 * The real-time timer, interrupting hz times per second.
 */
void
hardclock(struct clockframe *frame)
{
#if NDT > 0
	DT_ENTER(profile, NULL);
	if (CPU_IS_PRIMARY(curcpu()))
		DT_ENTER(interval, NULL);
#endif

	/*
	 * If we are not the primary CPU, we're not allowed to do
	 * any more work.
	 */
	if (CPU_IS_PRIMARY(curcpu()) == 0)
		return;

	tc_ticktock();
	ticks++;
	jiffies++;

	/*
	 * Update the timeout wheel.
	 */
	timeout_hardclock_update();
}

/*
 * Compute number of hz in the specified amount of time.
 */
int
tvtohz(const struct timeval *tv)
{
	unsigned long nticks;
	time_t sec;
	long usec;

	/*
	 * If the number of usecs in the whole seconds part of the time
	 * fits in a long, then the total number of usecs will
	 * fit in an unsigned long.  Compute the total and convert it to
	 * ticks, rounding up and adding 1 to allow for the current tick
	 * to expire.  Rounding also depends on unsigned long arithmetic
	 * to avoid overflow.
	 *
	 * Otherwise, if the number of ticks in the whole seconds part of
	 * the time fits in a long, then convert the parts to
	 * ticks separately and add, using similar rounding methods and
	 * overflow avoidance.  This method would work in the previous
	 * case but it is slightly slower and assumes that hz is integral.
	 *
	 * Otherwise, round the time down to the maximum
	 * representable value.
	 *
	 * If ints have 32 bits, then the maximum value for any timeout in
	 * 10ms ticks is 248 days.
	 */
	sec = tv->tv_sec;
	usec = tv->tv_usec;
	if (sec < 0 || (sec == 0 && usec <= 0))
		nticks = 0;
	else if (sec <= LONG_MAX / 1000000)
		nticks = (sec * 1000000 + (unsigned long)usec + (tick - 1))
		    / tick + 1;
	else if (sec <= LONG_MAX / hz)
		nticks = sec * hz
		    + ((unsigned long)usec + (tick - 1)) / tick + 1;
	else
		nticks = LONG_MAX;
	if (nticks > INT_MAX)
		nticks = INT_MAX;
	return ((int)nticks);
}

int
tstohz(const struct timespec *ts)
{
	struct timeval tv;
	TIMESPEC_TO_TIMEVAL(&tv, ts);

	/* Round up. */
	if ((ts->tv_nsec % 1000) != 0) {
		tv.tv_usec += 1;
		if (tv.tv_usec >= 1000000) {
			tv.tv_usec -= 1000000;
			tv.tv_sec += 1;
		}
	}

	return (tvtohz(&tv));
}

/*
 * Start profiling on a process.
 *
 * Kernel profiling passes proc0 which never exits and hence
 * keeps the profile clock running constantly.
 */
void
startprofclock(struct process *pr)
{
	int s;

	if ((pr->ps_flags & PS_PROFIL) == 0) {
		atomic_setbits_int(&pr->ps_flags, PS_PROFIL);
		if (++profprocs == 1) {
			s = splstatclock();
			setstatclockrate(profhz);
			splx(s);
		}
	}
}

/*
 * Stop profiling on a process.
 */
void
stopprofclock(struct process *pr)
{
	int s;

	if (pr->ps_flags & PS_PROFIL) {
		atomic_clearbits_int(&pr->ps_flags, PS_PROFIL);
		if (--profprocs == 0) {
			s = splstatclock();
			setstatclockrate(stathz);
			splx(s);
		}
	}
}

/*
 * Statistics clock.  Grab profile sample, and if divider reaches 0,
 * do process and kernel statistics.
 */
void
statclock(struct clockframe *frame)
{
	struct cpu_info *ci = curcpu();
	struct schedstate_percpu *spc = &ci->ci_schedstate;
	struct proc *p = curproc;
	struct process *pr;

	if (CLKF_USERMODE(frame)) {
		pr = p->p_p;
		/*
		 * Came from user mode; CPU was in user state.
		 * If this process is being profiled record the tick.
		 */
		p->p_uticks++;
		if (pr->ps_nice > NZERO)
			spc->spc_cp_time[CP_NICE]++;
		else
			spc->spc_cp_time[CP_USER]++;
	} else {
		/*
		 * Came from kernel mode, so we were:
		 * - spinning on a lock
		 * - handling an interrupt,
		 * - doing syscall or trap work on behalf of the current
		 *   user process, or
		 * - spinning in the idle loop.
		 * Whichever it is, charge the time as appropriate.
		 * Note that we charge interrupts to the current process,
		 * regardless of whether they are ``for'' that process,
		 * so that we know how much of its real time was spent
		 * in ``non-process'' (i.e., interrupt) work.
		 */
		if (CLKF_INTR(frame)) {
			if (p != NULL)
				p->p_iticks++;
			spc->spc_cp_time[spc->spc_spinning ?
			    CP_SPIN : CP_INTR]++;
		} else if (p != NULL && p != spc->spc_idleproc) {
			p->p_sticks++;
			spc->spc_cp_time[spc->spc_spinning ?
			    CP_SPIN : CP_SYS]++;
		} else
			spc->spc_cp_time[spc->spc_spinning ?
			    CP_SPIN : CP_IDLE]++;
	}

	if (p != NULL) {
		p->p_cpticks++;
		/*
		 * schedclock() runs every fourth statclock().
		 */
		if ((++spc->spc_schedticks & 3) == 0)
			schedclock(p);
	}
}

/*
 * Return information about system clocks.
 */
int
sysctl_clockrate(char *where, size_t *sizep, void *newp)
{
	struct clockinfo clkinfo;

	/*
	 * Construct clockinfo structure.
	 */
	memset(&clkinfo, 0, sizeof clkinfo);
	clkinfo.tick = tick;
	clkinfo.hz = hz;
	clkinfo.profhz = profhz;
	clkinfo.stathz = stathz;
	return (sysctl_rdstruct(where, sizep, newp, &clkinfo, sizeof(clkinfo)));
}
