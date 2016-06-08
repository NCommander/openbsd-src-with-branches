/*	$OpenBSD: platform.c,v 1.6 2016/06/04 18:09:16 jsg Exp $	*/
/*
 * Copyright (c) 2014 Patrick Wildt <patrick@blueri.se>
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
#include <sys/types.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <armv7/armv7/armv7var.h>
#include <armv7/armv7/armv7_machdep.h>
#include <arm/cortex/smc.h>

#include "imx.h"
#include "omap.h"
#include "sunxi.h"
#include "exynos.h"
#include "vexpress.h"

static struct armv7_platform *platform;

void	exuart_init_cons(void);
void	imxuart_init_cons(void);
void	omapuart_init_cons(void);
void	sxiuart_init_cons(void);
void	pl011_init_cons(void);

struct armv7_platform *imx_platform_match(void);
struct armv7_platform *omap_platform_match(void);
struct armv7_platform *sunxi_platform_match(void);
struct armv7_platform *exynos_platform_match(void);
struct armv7_platform *vexpress_platform_match(void);

struct armv7_platform * (*plat_match[])(void) = {
#if NIMX > 0
	imx_platform_match,
#endif
#if NOMAP > 0
	omap_platform_match,
#endif
#if NSUNXI > 0
	sunxi_platform_match,
#endif
#if NEXYNOS > 0
	exynos_platform_match,
#endif
#if NVEXPRESS > 0
	vexpress_platform_match,
#endif
};

void
platform_init(void)
{
	int i;
	
	for (i = 0; i < nitems(plat_match); i++) {
		platform = plat_match[i]();
		if (platform != NULL)
			break;
	}
	if (platform == NULL)
		panic("no matching armv7 platform");
	platform->board_init();
}

void
platform_smc_write(bus_space_tag_t iot, bus_space_handle_t ioh, bus_size_t off,
    uint32_t op, uint32_t val)
{
	platform->smc_write(iot, ioh, off, op, val);
}

void
platform_init_cons(void)
{
	if (platform && platform->init_cons) {
		platform->init_cons();
		return;
	}
	exuart_init_cons();
	imxuart_init_cons();
	omapuart_init_cons();
	sxiuart_init_cons();
	pl011_init_cons();
}

void
platform_init_mainbus(struct device *self)
{
	if (platform->init_mainbus)
		platform->init_mainbus(self);
}

void
platform_watchdog_reset(void)
{
	platform->watchdog_reset();
}

void
platform_powerdown(void)
{
	platform->powerdown();
}

void
platform_disable_l2_if_needed(void)
{
	platform->disable_l2_if_needed();
}

struct board_dev *
platform_board_devs()
{
	return (platform->devs);
}
