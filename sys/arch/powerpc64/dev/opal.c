/*	$OpenBSD: opal.c,v 1.5 2020/06/26 19:06:35 kettenis Exp $	*/
/*
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/fdt.h>
#include <machine/opal.h>

#include <dev/clock_subr.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>

#define OPAL_NUM_HANDLERS	4

struct opal_intr {
	struct opal_softc	*oi_sc;
	uint32_t		oi_isn;
};

struct intrhand {
	uint64_t		ih_events;
	int			(*ih_func)(void *);
	void			*ih_arg;
};

struct opal_softc {
	struct device		sc_dev;

	struct opal_intr	*sc_intr;
	int			sc_nintr;

	struct intrhand		*sc_handler[OPAL_NUM_HANDLERS];

	struct todr_chip_handle	sc_todr;
};

struct opal_softc *opal_sc;

int	opal_match(struct device *, void *, void *);
void	opal_attach(struct device *, struct device *, void *);

struct cfattach	opal_ca = {
	sizeof (struct opal_softc), opal_match, opal_attach
};

struct cfdriver opal_cd = {
	NULL, "opal", DV_DULL
};

void	opal_attach_deferred(struct device *);
void	opal_attach_node(struct opal_softc *, int);
int	opal_gettime(struct todr_chip_handle *, struct timeval *);
int	opal_settime(struct todr_chip_handle *, struct timeval *);

int
opal_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "ibm,opal-v3");
}

void
opal_attach(struct device *parent, struct device *self, void *aux)
{
	struct opal_softc *sc = (struct opal_softc *)self;
	struct fdt_attach_args *faa = aux;
	uint32_t *interrupts;
	int len, i;
	int node;

	node = OF_getnodebyname(faa->fa_node, "firmware");
	if (node) {
		char version[64];

		version[0] = 0;
		OF_getprop(node, "version", version, sizeof(version));
		version[sizeof(version) - 1] = 0;
		printf(": %s", version);
	}

	len = OF_getproplen(faa->fa_node, "opal-interrupts");
	if (len > 0 && (len % sizeof(uint32_t)) != 0) {
		printf(": can't parse interrupts\n");
		return;
	}
		
	printf("\n");

	/* There can be only one. */
	KASSERT(opal_sc == NULL);
	opal_sc = sc;

	if (len > 0) {
		interrupts = malloc(len, M_TEMP, M_WAITOK);
		OF_getpropintarray(faa->fa_node, "opal-interrupts",
		    interrupts, len);
		sc->sc_nintr = len / sizeof(uint32_t);

		sc->sc_intr = mallocarray(sc->sc_nintr,
		    sizeof(struct opal_intr), M_DEVBUF, M_WAITOK);

		for (i = 0; i < sc->sc_nintr; i++) {
			sc->sc_intr[i].oi_sc = sc;
			sc->sc_intr[i].oi_isn = interrupts[i];
		}

		free(interrupts, M_TEMP, len);

		config_defer(self, opal_attach_deferred);
	}

	sc->sc_todr.todr_gettime = opal_gettime;
	sc->sc_todr.todr_settime = opal_settime;
	todr_attach(&sc->sc_todr);

	node = OF_getnodebyname(faa->fa_node, "consoles");
	if (node) {
		for (node = OF_child(node); node; node = OF_peer(node))
			opal_attach_node(sc, node);
	}
}

int
opal_intr(void *arg)
{
	struct opal_intr *oi = arg;
	struct opal_softc *sc = oi->oi_sc;
	uint64_t events = 0;
	int i;

	opal_handle_interrupt(oi->oi_isn, opal_phys(&events));

	/* Handle registered events. */
	for (i = 0; i < OPAL_NUM_HANDLERS; i++) {
		struct intrhand *ih = sc->sc_handler[i];

		if (ih == NULL)
			continue;
		if ((events & ih->ih_events) == 0)
			continue;

		ih->ih_func(ih->ih_arg);
	}

	return 1;
}

void *
opal_intr_establish(uint64_t events, int level, int (*func)(void *), void *arg)
{
	struct opal_softc *sc = opal_sc;
	struct intrhand *ih;
	int i;

	for (i = 0; i < OPAL_NUM_HANDLERS; i++) {
		if (sc->sc_handler[i] == NULL)
			break;
	}
	if (i == OPAL_NUM_HANDLERS)
		return NULL;

	ih = malloc(sizeof(*ih), M_DEVBUF, M_WAITOK);
	ih->ih_events = events;
	ih->ih_func = func;
	ih->ih_arg = arg;
	sc->sc_handler[i] = ih;

	return ih;
}

void
opal_attach_deferred(struct device *self)
{
	struct opal_softc *sc = (struct opal_softc *)self;
	int i;

	for (i = 0; i < sc->sc_nintr; i++) {
		intr_establish(sc->sc_intr[i].oi_isn, IST_LEVEL, IPL_TTY,
		    opal_intr, &sc->sc_intr[i], sc->sc_dev.dv_xname);
	}
}

int
opal_print(void *aux, const char *pnp)
{
	struct fdt_attach_args *faa = aux;
	char name[32];

	if (!pnp)
		return (QUIET);

	if (OF_getprop(faa->fa_node, "name", name, sizeof(name)) > 0) {
		name[sizeof(name) - 1] = 0;
		printf("\"%s\"", name);
	} else
		printf("node %u", faa->fa_node);

	printf(" at %s", pnp);

	return (UNCONF);
}

void
opal_attach_node(struct opal_softc *sc, int node)
{
	struct fdt_attach_args faa;
	char buf[32];

	if (OF_getproplen(node, "compatible") <= 0)
		return;

	if (OF_getprop(node, "status", buf, sizeof(buf)) > 0 &&
	    strcmp(buf, "disabled") == 0)
		return;

	memset(&faa, 0, sizeof(faa));
	faa.fa_name = "";
	faa.fa_node = node;

	config_found(&sc->sc_dev, &faa, opal_print);
}

int
opal_gettime(struct todr_chip_handle *ch, struct timeval *tv)
{
	struct clock_ymdhms dt;
	uint64_t time;
	uint32_t date;
	int64_t error;

	do {
		error = opal_rtc_read(opal_phys(&date), opal_phys(&time));
		if (error == OPAL_BUSY_EVENT)
			opal_poll_events(NULL);
	} while (error == OPAL_BUSY_EVENT);

	if (error != OPAL_SUCCESS)
		return EIO;

	dt.dt_sec = FROMBCD((time >> 40) & 0xff);
	dt.dt_min = FROMBCD((time >> 48) & 0xff);
	dt.dt_hour = FROMBCD((time >> 56) & 0xff);
	dt.dt_day = FROMBCD((date >> 0) & 0xff);
	dt.dt_mon = FROMBCD((date >> 8) & 0xff);
	dt.dt_year = FROMBCD((date >> 16) & 0xff);
	dt.dt_year += 100 * FROMBCD((date >> 24) & 0xff);

	tv->tv_sec = clock_ymdhms_to_secs(&dt);
	tv->tv_usec = 0;

	return 0;
}

int
opal_settime(struct todr_chip_handle *ch, struct timeval *tv)
{
	struct clock_ymdhms dt;
	uint64_t time = 0;
	uint32_t date = 0;
	int64_t error;

	clock_secs_to_ymdhms(tv->tv_sec, &dt);

	time |= (uint64_t)TOBCD(dt.dt_sec) << 40;
	time |= (uint64_t)TOBCD(dt.dt_min) << 48;
	time |= (uint64_t)TOBCD(dt.dt_hour) << 56;
	date |= (uint32_t)TOBCD(dt.dt_day);
	date |= (uint32_t)TOBCD(dt.dt_mon) << 8;
	date |= (uint32_t)TOBCD(dt.dt_year) << 16;
	date |= (uint32_t)TOBCD(dt.dt_year / 100) << 24;

	do {
		error = opal_rtc_write(date, time);
		if (error == OPAL_BUSY_EVENT)
			opal_poll_events(NULL);
	} while (error == OPAL_BUSY_EVENT);

	if (error != OPAL_SUCCESS)
		return EIO;

	return 0;
}
