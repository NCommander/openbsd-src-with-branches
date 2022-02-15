/* $OpenBSD: subr_suspend.c,v 1.5 2022/02/15 02:38:18 deraadt Exp $ */
/*
 * Copyright (c) 2005 Thorsten Lockert <tholo@sigmasoft.com>
 * Copyright (c) 2005 Jordan Hargrave <jordan@openbsd.org>
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
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/reboot.h>
#include <sys/proc.h>
#include <sys/sensors.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <dev/wscons/wsdisplayvar.h>
#ifdef HIBERNATE
#include <sys/hibernate.h>
#endif

#include "softraid.h"

int
sleep_state(void *v, int sleepmode)
{
	int error = ENXIO;
	extern int perflevel;
	size_t rndbuflen = 0;
	char *rndbuf = NULL;
	int s;
#if NSOFTRAID > 0
	extern void sr_quiesce(void);
#endif

	if (sleep_showstate(v, sleepmode))
		return EOPNOTSUPP;

	display_suspend(v);

	stop_periodic_resettodr();

#ifdef HIBERNATE
	if (sleepmode == SLEEP_HIBERNATE) {
		/*
		 * Discard useless memory to reduce fragmentation,
		 * and attempt to create a hibernate work area
		 */
		hibernate_suspend_bufcache();
		uvmpd_hibernate();
		if (hibernate_alloc()) {
			printf("failed to allocate hibernate memory\n");
			sleep_abort(v);
			goto fail_alloc;
		}
	}
#endif /* HIBERNATE */

	sensor_quiesce();
	if (config_suspend_all(DVACT_QUIESCE)) {
		sleep_abort(v);
		goto fail_quiesce;
	}

	vfs_stall(curproc, 1);
#if NSOFTRAID > 0
	sr_quiesce();
#endif
	bufq_quiesce();

	
#ifdef MULTIPROCESSOR
	sched_stop_secondary_cpus();
	KASSERT(CPU_IS_PRIMARY(curcpu()));
	sleep_mp();
#endif

#ifdef HIBERNATE
	if (sleepmode == SLEEP_HIBERNATE) {
		/*
		 * We've just done various forms of syncing to disk
		 * churned lots of memory dirty.  We don't need to
		 * save that dirty memory to hibernate, so release it.
		 */
		hibernate_suspend_bufcache();
		uvmpd_hibernate();
	}
#endif /* HIBERNATE */

	resettodr();

	s = splhigh();
	intr_disable();	/* PSL_I for resume; PIC/APIC broken until repair */
	cold = 2;	/* Force other code to delay() instead of tsleep() */

	if (config_suspend_all(DVACT_SUSPEND) != 0) {
		sleep_abort(v);
		goto fail_suspend;
	}

	suspend_randomness();

	if (sleep_setstate(v)) {
		sleep_abort(v);
		goto fail_pts;
	}

	if (sleepmode == SLEEP_SUSPEND) {
		/*
		 * XXX
		 * Flag to disk drivers that they should "power down" the disk
		 * when we get to DVACT_POWERDOWN.
		 */
		boothowto |= RB_POWERDOWN;
		config_suspend_all(DVACT_POWERDOWN);
		boothowto &= ~RB_POWERDOWN;
	}

	gosleep(v);

#ifdef HIBERNATE
	if (sleepmode == SLEEP_HIBERNATE) {
		uvm_pmr_dirty_everything();
		hib_getentropy(&rndbuf, &rndbuflen);
	}
#endif /* HIBERNATE */

fail_pts:
	config_suspend_all(DVACT_RESUME);

fail_suspend:
	cold = 0;
	intr_enable();
	splx(s);

	inittodr(gettime());

	sleep_resume(v);

	/* force RNG upper level reseed */
	resume_randomness(rndbuf, rndbuflen);

#ifdef MULTIPROCESSOR
	resume_mp();
	sched_start_secondary_cpus();
#endif

	vfs_stall(curproc, 0);
	bufq_restart();

fail_quiesce:
	config_suspend_all(DVACT_WAKEUP);
	sensor_restart();

#ifdef HIBERNATE
	if (sleepmode == SLEEP_HIBERNATE) {
		hibernate_free();
fail_alloc:
		hibernate_resume_bufcache();
	}
#endif /* HIBERNATE */

	start_periodic_resettodr();

	display_resume(v);

	sys_sync(curproc, NULL, NULL);

	/* Restore hw.setperf */
	if (cpu_setperf != NULL)
		cpu_setperf(perflevel);

	suspend_finish(v);

	return (error);
}
