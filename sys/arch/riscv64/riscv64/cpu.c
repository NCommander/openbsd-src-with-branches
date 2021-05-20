/*	$OpenBSD: cpu.c,v 1.7 2021/05/14 06:48:52 jsg Exp $	*/

/*
 * Copyright (c) 2016 Dale Rahn <drahn@dalerahn.com>
 * Copyright (c) 2017 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/sysctl.h>

#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_clock.h>
#include <dev/ofw/fdt.h>

register_t mvendorid, marchid, mimpid;
char cpu_model[64];
int cpu_node;

struct cpu_info *cpu_info_list = &cpu_info_primary;

int	cpu_match(struct device *, void *, void *);
void	cpu_attach(struct device *, struct device *, void *);

struct cfattach cpu_ca = {
	sizeof(struct device), cpu_match, cpu_attach
};

struct cfdriver cpu_cd = {
	NULL, "cpu", DV_DULL
};

void
cpu_identify(struct cpu_info *ci)
{
	char isa[32];
	int len;

	len = OF_getprop(ci->ci_node, "riscv,isa", isa, sizeof(isa));

	printf(": vendor %lx arch %lx imp %lx",
	    mvendorid, marchid, mimpid);

	if (len != -1) {
		printf(" %s", isa);
		strlcpy(cpu_model, isa, sizeof(cpu_model));
	}
	printf("\n");
}

#ifdef MULTIPROCESSOR
int	cpu_hatch_secondary(struct cpu_info *ci, int, uint64_t);
#endif
int	cpu_clockspeed(int *);

int
cpu_match(struct device *parent, void *cfdata, void *aux)
{
	struct fdt_attach_args *faa = aux;
	char buf[32];

	if (OF_getprop(faa->fa_node, "device_type", buf, sizeof(buf)) <= 0 ||
	    strcmp(buf, "cpu") != 0)
		return 0;

	if (ncpus < MAXCPUS || faa->fa_reg[0].addr == boot_hart) /* the primary cpu */
		return 1;

	return 0;
}

void
cpu_attach(struct device *parent, struct device *dev, void *aux)
{
	struct fdt_attach_args *faa = aux;
	struct cpu_info *ci;
	int node, level;

	KASSERT(faa->fa_nreg > 0);

	if (faa->fa_reg[0].addr == boot_hart) {/* the primary cpu */
		ci = &cpu_info_primary;
#ifdef MULTIPROCESSOR
		ci->ci_flags |= CPUF_RUNNING | CPUF_PRESENT | CPUF_PRIMARY;
#endif
	}
#ifdef MULTIPROCESSOR
	else {
		ci = malloc(sizeof(*ci), M_DEVBUF, M_WAITOK | M_ZERO);
		cpu_info[dev->dv_unit] = ci;
		ci->ci_next = cpu_info_list->ci_next;
		cpu_info_list->ci_next = ci;
		ci->ci_flags |= CPUF_AP;
		ncpus++;
	}
#endif

	ci->ci_dev = dev;
	ci->ci_cpuid = dev->dv_unit;
	ci->ci_node = faa->fa_node;
	ci->ci_self = ci;

#ifdef MULTIPROCESSOR
	if (ci->ci_flags & CPUF_AP) {
		char buf[32];
		uint64_t spinup_data = 0;
		int spinup_method = 0;
		int timeout = 10000;
		int len;

		len = OF_getprop(ci->ci_node, "enable-method",
		    buf, sizeof(buf));
		if (strcmp(buf, "psci") == 0) {
			spinup_method = 1;
		} else if (strcmp(buf, "spin-table") == 0) {
			spinup_method = 2;
			spinup_data = OF_getpropint64(ci->ci_node,
			    "cpu-release-addr", 0);
		}

		sched_init_cpu(ci);
		if (cpu_hatch_secondary(ci, spinup_method, spinup_data)) {
			atomic_setbits_int(&ci->ci_flags, CPUF_IDENTIFY);
			__asm volatile("dsb sy; sev");

			while ((ci->ci_flags & CPUF_IDENTIFIED) == 0 &&
			    --timeout)
				delay(1000);
			if (timeout == 0) {
				printf(" failed to identify");
				ci->ci_flags = 0;
			}
		} else {
			printf(" failed to spin up");
			ci->ci_flags = 0;
		}
	} else {
#endif
		cpu_identify(ci);

		if (OF_getproplen(ci->ci_node, "clocks") > 0) {
			cpu_node = ci->ci_node;
			cpu_cpuspeed = cpu_clockspeed;
		}

		/*
		 * attach cpu's children node, so far there is only the
		 * cpu-embedded interrupt controller
		 */
		struct fdt_attach_args	 fa_intc;
		for (node = OF_child(faa->fa_node); node; node = OF_peer(node)) {
			fa_intc.fa_node = node;
			/* no specifying match func, will call cfdata's match func*/
			config_found(dev, &fa_intc, NULL);
		}

#ifdef MULTIPROCESSOR
	}
#endif

	node = faa->fa_node;

	level = 1;

	while (node) {
		const char *unit = "KB";
		uint32_t line, iline, dline;
		uint32_t size, isize, dsize;
		uint32_t ways, iways, dways;
		uint32_t cache;

		line = OF_getpropint(node, "cache-block-size", 0);
		size = OF_getpropint(node, "cache-size", 0);
		ways = OF_getpropint(node, "cache-sets", 0);
		iline = OF_getpropint(node, "i-cache-block-size", line);
		isize = OF_getpropint(node, "i-cache-size", size) / 1024;
		iways = OF_getpropint(node, "i-cache-sets", ways);
		dline = OF_getpropint(node, "d-cache-block-size", line);
		dsize = OF_getpropint(node, "d-cache-size", size) / 1024;
		dways = OF_getpropint(node, "d-cache-sets", ways);

		if (isize == 0 && dsize == 0)
			break;

		/* Print large cache sizes in MB. */
		if (isize > 4096 && dsize > 4096) {
			unit = "MB";
			isize /= 1024;
			dsize /= 1024;
		}

		printf("%s:", dev->dv_xname);
		
		if (OF_getproplen(node, "cache-unified") == 0) {
			printf(" %d%s %db/line %d-way L%d cache",
			    isize, unit, iline, iways, level);
		} else {
			printf(" %d%s %db/line %d-way L%d I-cache",
			    isize, unit, iline, iways, level);
			printf(", %d%s %db/line %d-way L%d D-cache",
			    dsize, unit, dline, dways, level);
		}

		cache = OF_getpropint(node, "next-level-cache", 0);
		node = OF_getnodebyphandle(cache);
		level++;

		printf("\n");
	}
}

int
cpu_clockspeed(int *freq)
{
	*freq = clock_get_frequency(cpu_node, NULL) / 1000000;
	return 0;
}

void
cpu_cache_nop_range(paddr_t pa, psize_t len)
{
}

void (*cpu_dcache_wbinv_range)(paddr_t, psize_t) = cpu_cache_nop_range;
void (*cpu_dcache_inv_range)(paddr_t, psize_t) = cpu_cache_nop_range;
void (*cpu_dcache_wb_range)(paddr_t, psize_t) = cpu_cache_nop_range;
