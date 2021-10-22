/*	$OpenBSD: clock.c,v 1.2 2020/07/12 20:32:20 naddy Exp $	*/

/*
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
 * Copyright (c) 2003 Dale Rahn <drahn@openbsd.org>
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
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/evcount.h>
#include <sys/timetc.h>

#include <machine/cpufunc.h>

extern uint64_t tb_freq;	/* cpu.c */

uint64_t tick_increment;
uint64_t statmin;
uint32_t statvar;

struct evcount clock_count;
struct evcount stat_count;

u_int	tb_get_timecount(struct timecounter *);

static struct timecounter tb_timecounter = {
	.tc_get_timecount = tb_get_timecount,
	.tc_poll_pps = NULL,
	.tc_counter_mask = 0xffffffff,
	.tc_frequency = 0,
	.tc_name = "tb",
	.tc_quality = 0,
	.tc_priv = NULL,
};

void	cpu_startclock(void);

u_int
tb_get_timecount(struct timecounter *tc)
{
	return mftb();
}

void
cpu_initclocks(void)
{
	tick_increment = tb_freq / hz;

	stathz = 100;
	profhz = 1000; /* must be a multiple of stathz */

	setstatclockrate(stathz);

	evcount_attach(&clock_count, "clock", NULL);
	evcount_attach(&stat_count, "stat", NULL);

	cpu_startclock();

	tb_timecounter.tc_frequency = tb_freq;
	tc_init(&tb_timecounter);
}

void
cpu_startclock(void)
{
	struct cpu_info *ci = curcpu();
	uint64_t nextevent;

	ci->ci_lasttb = mftb();
	ci->ci_nexttimerevent = ci->ci_lasttb + tick_increment;
	nextevent = ci->ci_nextstatevent = ci->ci_nexttimerevent;

	mtdec(nextevent - ci->ci_lasttb);
	intr_enable();
}

void
decr_intr(struct trapframe *frame)
{
	struct cpu_info *ci = curcpu();
	uint64_t tb, prevtb;
	uint64_t nextevent;
	uint32_t r;
	int nstats;
	int s;

	/*
	 * Based on the actual time delay since the last decrementer reload,
	 * we arrange for earlier interrupt next time.
	 */

	tb = mftb();

	while (ci->ci_nexttimerevent <= tb)
		ci->ci_nexttimerevent += tick_increment;

	prevtb = ci->ci_nexttimerevent - tick_increment;

	for (nstats = 0; ci->ci_nextstatevent <= tb; nstats++) {
		do {
			r = random() & (statvar - 1);
		} while (r == 0); /* random == 0 not allowed */
		ci->ci_nextstatevent += statmin + r;
	}
	stat_count.ec_count += nstats;

	if (ci->ci_nexttimerevent < ci->ci_nextstatevent)
		nextevent = ci->ci_nexttimerevent;
	else
		nextevent = ci->ci_nextstatevent;

	/* 
	 * Transition of the MSB will trigger a decrementer interrupt.
	 * So the next sequence is guaranteed to do the job without a
	 * systematic skew.
	 */
	mtdec(nextevent - tb);
	mtdec(nextevent - mftb());

	if (ci->ci_cpl >= IPL_CLOCK) {
		ci->ci_statspending += nstats;
	} else {
		nstats += ci->ci_statspending;
		ci->ci_statspending = 0;

		s = splclock();
		intr_enable();

		/*
		 * Do standard timer interrupt stuff.
		 */
		while (ci->ci_lasttb < prevtb) {
			ci->ci_lasttb += tick_increment;
			clock_count.ec_count++;
			hardclock((struct clockframe *)frame);
		}

		while (nstats-- > 0)
			statclock((struct clockframe *)frame);

		intr_disable();
		splx(s);
	}
}

void
setstatclockrate(int newhz)
{
	uint64_t stat_increment;
	uint64_t min_increment;
	uint32_t var;
	u_long msr;

	msr = intr_disable();

	stat_increment = tb_freq / newhz;
	var = 0x40000000; /* really big power of two */
	/* Find largest 2^n which is nearly smaller than statint/2. */
	min_increment = stat_increment / 2 + 100;
	while (var > min_increment)
		var >>= 1;

	/* Not atomic, but we can probably live with that. */
	statmin = stat_increment - (var >> 1);
	statvar = var;

	intr_restore(msr);
}

void
delay(u_int us)
{
	uint64_t tb;

	tb = mftb();
	tb += (us * tb_freq + 999999) / 1000000;
	while (tb > mftb())
		continue;
}
