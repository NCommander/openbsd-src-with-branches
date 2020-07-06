/*	$OpenBSD: clock.c,v 1.43 2020/05/01 20:00:26 kettenis Exp $	*/
/*	$NetBSD: clock.c,v 1.1 1996/09/30 16:34:40 ws Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/evcount.h>
#include <sys/timetc.h>

#include <machine/autoconf.h>
#include <machine/pio.h>
#include <machine/intr.h>
#include <machine/vmparam.h>

#include <dev/clock_subr.h>
#include <dev/ofw/openfirm.h>

void decr_intr(struct clockframe *frame);
u_int tb_get_timecount(struct timecounter *);

/*
 * Initially we assume a processor with a bus frequency of 12.5 MHz.
 */
u_int32_t ticks_per_sec = 3125000;
u_int32_t ns_per_tick = 320;
static int32_t ticks_per_intr;

static struct timecounter tb_timecounter = {
	tb_get_timecount, NULL, 0x7fffffff, 0, "tb", 0, NULL, 0
};

/* calibrate the timecounter frequency for the listed models */
static const char *calibrate_tc_models[] = {
	"PowerMac10,1"
};
extern char *hw_prod;

time_read_t  *time_read;
time_write_t *time_write;

/* vars for stats */
int statint;
u_int32_t statvar;
u_int32_t statmin;

static struct evcount clk_count;
static struct evcount stat_count;
static int clk_irq = PPC_CLK_IRQ;
static int stat_irq = PPC_STAT_IRQ;

extern todr_chip_handle_t todr_handle;
struct todr_chip_handle rtc_todr;

int
rtc_gettime(struct todr_chip_handle *handle, struct timeval *tv)
{
	time_t sec;

	if (time_read == NULL)
		return ENXIO;

	(*time_read)(&sec);
	tv->tv_sec = sec - utc_offset;
	tv->tv_usec = 0;
	return 0;
}

int
rtc_settime(struct todr_chip_handle *handle, struct timeval *tv)
{
	if (time_write == NULL)
		return ENXIO;

	(*time_write)(tv->tv_sec + utc_offset);
	return 0;
}

void
decr_intr(struct clockframe *frame)
{
	u_int64_t tb;
	u_int64_t nextevent;
	struct cpu_info *ci = curcpu();
	int nstats;
	int s;

	/*
	 * Check whether we are initialized.
	 */
	if (!ticks_per_intr)
		return;

	/*
	 * Based on the actual time delay since the last decrementer reload,
	 * we arrange for earlier interrupt next time.
	 */

	tb = ppc_mftb();
	while (ci->ci_nexttimerevent <= tb)
		ci->ci_nexttimerevent += ticks_per_intr;

	ci->ci_prevtb = ci->ci_nexttimerevent - ticks_per_intr;

	for (nstats = 0; ci->ci_nextstatevent <= tb; nstats++) {
		int r;
		do {
			r = random() & (statvar -1);
		} while (r == 0); /* random == 0 not allowed */
		ci->ci_nextstatevent += statmin + r;
	}

	/* only count timer ticks for CLK_IRQ */
	stat_count.ec_count += nstats;

	if (ci->ci_nexttimerevent < ci->ci_nextstatevent)
		nextevent = ci->ci_nexttimerevent;
	else
		nextevent = ci->ci_nextstatevent;

	/*
	 * Need to work about the near constant skew this introduces???
	 * reloading tb here could cause a missed tick.
	 */
	ppc_mtdec(nextevent - tb);

	if (ci->ci_cpl >= IPL_CLOCK) {
		ci->ci_statspending += nstats;
	} else {
		nstats += ci->ci_statspending;
		ci->ci_statspending = 0;

		s = splclock();

		/*
		 * Reenable interrupts
		 */
		ppc_intr_enable(1);

		/*
		 * Do standard timer interrupt stuff.
		 */
		while (ci->ci_lasttb < ci->ci_prevtb) {
			/* sync lasttb with hardclock */
			ci->ci_lasttb += ticks_per_intr;
			clk_count.ec_count++;
			hardclock(frame);
		}

		while (nstats-- > 0)
			statclock(frame);

		splx(s);
		(void) ppc_intr_disable();

		/* if a tick has occurred while dealing with these,
		 * dont service it now, delay until the next tick.
		 */
	}
}

void cpu_startclock(void);

void
cpu_initclocks(void)
{
	int intrstate;
	int minint;
	u_int32_t first_tb, second_tb;
	time_t first_sec, sec;
	int calibrate = 0, n;

	/* check if we should calibrate the timecounter frequency */
	for (n = 0; n < sizeof(calibrate_tc_models) /
	    sizeof(calibrate_tc_models[0]); n++) {
		if (!strcmp(calibrate_tc_models[n], hw_prod)) {
			calibrate = 1;
			break;
		}
	}

	/* if a RTC is available, calibrate the timecounter frequency */
	if (calibrate && time_read != NULL) {
		time_read(&first_sec);
		do {
			first_tb = ppc_mftbl();
			time_read(&sec);
		} while (sec == first_sec);
		first_sec = sec;
		do {
			second_tb = ppc_mftbl();
			time_read(&sec);
		} while (sec == first_sec);
		ticks_per_sec = second_tb - first_tb;
#ifdef DEBUG
		printf("tb: using measured timecounter frequency of %ld Hz\n",
		    ticks_per_sec);
#endif
	}

	rtc_todr.todr_gettime = rtc_gettime;
	rtc_todr.todr_settime = rtc_settime;
	todr_handle = &rtc_todr;

	intrstate = ppc_intr_disable();

	ticks_per_intr = ticks_per_sec / hz;

	stathz = 100;
	profhz = 1000; /* must be a multiple of stathz */

	/* init secondary clock to stathz */
	statint = ticks_per_sec / stathz;
	statvar = 0x40000000; /* really big power of two */
	/* find largest 2^n which is nearly smaller than statint/2  */
	minint = statint / 2 + 100;
	while (statvar > minint)
		statvar >>= 1;
	statmin = statint - (statvar >> 1);

	evcount_attach(&clk_count, "clock", &clk_irq);
	evcount_attach(&stat_count, "stat", &stat_irq);

	cpu_startclock();

	tb_timecounter.tc_frequency = ticks_per_sec;
	tc_init(&tb_timecounter);
	ppc_intr_enable(intrstate);
}

void
cpu_startclock(void)
{
	struct cpu_info *ci = curcpu();
	u_int64_t nextevent;

	ci->ci_lasttb = ppc_mftb();

	/*
	 * no point in having random on the first tick, 
	 * it just complicates the code.
	 */
	ci->ci_nexttimerevent = ci->ci_lasttb + ticks_per_intr;
	nextevent = ci->ci_nextstatevent = ci->ci_nexttimerevent;

	ci->ci_statspending = 0;

	ppc_mtdec(nextevent - ci->ci_lasttb);
}

/*
 * Wait for about n microseconds (us) (at least!).
 */
void
delay(unsigned n)
{
	u_int64_t tb;

	tb = ppc_mftb();
	tb += (n * 1000 + ns_per_tick - 1) / ns_per_tick;
	while (tb > ppc_mftb())
		;
}

/*
 * Nothing to do.
 */
void
setstatclockrate(int newhz)
{
	int minint;
	int intrstate;

	intrstate = ppc_intr_disable();

	statint = ticks_per_sec / newhz;
	statvar = 0x40000000; /* really big power of two */
	/* find largest 2^n which is nearly smaller than statint/2 */
	minint = statint / 2 + 100;
	while (statvar > minint)
		statvar >>= 1;

	statmin = statint - (statvar >> 1);
	ppc_intr_enable(intrstate);

	/*
	 * XXX this allows the next stat timer to occur then it switches
	 * to the new frequency. Rather than switching instantly.
	 */
}

u_int
tb_get_timecount(struct timecounter *tc)
{
	return ppc_mftbl();
}
