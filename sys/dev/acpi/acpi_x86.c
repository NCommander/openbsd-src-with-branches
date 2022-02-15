/* $OpenBSD: acpi_x86.c,v 1.3 2022/02/14 23:42:37 kettenis Exp $ */
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
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/event.h>
#include <sys/signalvar.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/sched.h>
#include <sys/reboot.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/sensors.h>
#include <sys/timetc.h>

#ifdef HIBERNATE
#include <sys/hibernate.h>
#endif

#include <machine/conf.h>
#include <machine/cpufunc.h>
#include <machine/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/dsdt.h>
#include <dev/wscons/wsdisplayvar.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/ppbreg.h>

#include <dev/pci/pciidevar.h>

#include <machine/apmvar.h>
#define APMUNIT(dev)	(minor(dev)&0xf0)
#define APMDEV(dev)	(minor(dev)&0x0f)
#define APMDEV_NORMAL	0
#define APMDEV_CTL	8

#include "wd.h"
#include "wsdisplay.h"
#include "softraid.h"

int
sleep_showstate(void *v, int sleepmode)
{
	struct acpi_softc *sc = v;

	switch (sleepmode) {
	case SLEEP_SUSPEND:
		sc->sc_state = ACPI_STATE_S3;
		break;
	case SLEEP_HIBERNATE:
		sc->sc_state = ACPI_STATE_S4;
		break;
	default:
		return (EOPNOTSUPP);
	}

	if (sc->sc_sleeptype[sc->sc_state].slp_typa == -1 ||
	    sc->sc_sleeptype[sc->sc_state].slp_typb == -1) {
		printf("%s: state S%d unavailable\n",
		    sc->sc_dev.dv_xname, sc->sc_state);
		return (EOPNOTSUPP);
	}

	/* 1st suspend AML step: _TTS(tostate) */
	if (aml_node_setval(sc, sc->sc_tts, sc->sc_state) != 0)
		return (EINVAL);
	acpi_indicator(sc, ACPI_SST_WAKING);    /* blink */
	return 0;
}

int
sleep_setstate(void *v)
{
	struct acpi_softc *sc = v;

	/* 2nd suspend AML step: _PTS(tostate) */
	if (aml_node_setval(sc, sc->sc_pts, sc->sc_state) != 0)
		return (EINVAL);
	acpi_indicator(sc, ACPI_SST_WAKING);    /* blink */

	acpibtn_enable_psw();   /* enable _LID for wakeup */
	acpi_indicator(sc, ACPI_SST_SLEEPING);

	/* 3rd suspend AML step: _GTS(tostate) */
	aml_node_setval(sc, sc->sc_gts, sc->sc_state);
	return 0;
}

void
gosleep(void *v)
{
	struct acpi_softc *sc = v;

	/* Clear fixed event status */
	acpi_write_pmreg(sc, ACPIREG_PM1_STS, 0, ACPI_PM1_ALL_STS);

	/* Enable wake GPEs */
	acpi_disable_allgpes(sc);
	acpi_enable_wakegpes(sc, sc->sc_state);

	/* Sleep */
	acpi_sleep_cpu(sc, sc->sc_state);
	/* Resume */

	acpi_resume_cpu(sc, sc->sc_state);
	sc->sc_state = ACPI_STATE_S0;
}

int
sleep_resume(void *v)
{
	struct acpi_softc *sc = v;

	/* 3rd resume AML step: _TTS(runstate) */
	if (aml_node_setval(sc, sc->sc_tts, sc->sc_state) != 0)
		return (EINVAL);
	acpi_indicator(sc, ACPI_SST_WAKING);    /* blink */
	return 0;
}

void
suspend_finish(void *v)
{
	struct acpi_softc *sc = v;
	extern int lid_action;

	acpi_record_event(sc, APM_NORMAL_RESUME);
	acpi_indicator(sc, ACPI_SST_WORKING);

	/* If we woke up but all the lids are closed, go back to sleep */
	if (acpibtn_numopenlids() == 0 && lid_action != 0)
		acpi_addtask(sc, acpi_sleep_task, sc, sc->sc_state);
}

void
disable_lid_wakeups(void *v)
{
	acpibtn_disable_psw();		/* disable _LID for wakeup */

}

void
display_suspend(void *v)
{
#if NWSDISPLAY > 0
	struct acpi_softc *sc = v;

	/*
	 * Temporarily release the lock to prevent the X server from
	 * blocking on setting the display brightness.
	 */
	rw_exit_write(&sc->sc_lck);
	wsdisplay_suspend();
	rw_enter_write(&sc->sc_lck);
#endif /* NWSDISPLAY > 0 */
}

void
display_resume(void *v)
{
#if NWSDISPLAY > 0
	struct acpi_softc *sc = v;

	rw_exit_write(&sc->sc_lck);
	wsdisplay_resume();
	rw_enter_write(&sc->sc_lck);
#endif /* NWSDISPLAY > 0 */
}
