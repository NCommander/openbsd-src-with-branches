/* $OpenBSD: agintc.c,v 1.54 2023/09/22 01:10:43 jsg Exp $ */
/*
 * Copyright (c) 2007, 2009, 2011, 2017 Dale Rahn <drahn@dalerahn.com>
 * Copyright (c) 2018 Mark Kettenis <kettenis@openbsd.org>
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

/*
 * This is a device driver for the GICv3/GICv4 IP from ARM as specified
 * in IHI0069C, an example of this hardware is the GIC 500.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/evcount.h>

#include <machine/bus.h>
#include <machine/cpufunc.h>
#include <machine/fdt.h>

#include <dev/ofw/fdt.h>
#include <dev/ofw/openfirm.h>

#include <machine/simplebusvar.h>

#define ICC_PMR		s3_0_c4_c6_0
#define ICC_IAR0	s3_0_c12_c8_0
#define ICC_EOIR0	s3_0_c12_c8_1
#define ICC_HPPIR0	s3_0_c12_c8_2
#define ICC_BPR0	s3_0_c12_c8_3

#define ICC_DIR		s3_0_c12_c11_1
#define ICC_RPR		s3_0_c12_c11_3
#define ICC_SGI1R	s3_0_c12_c11_5
#define ICC_SGI0R	s3_0_c12_c11_7

#define ICC_IAR1	s3_0_c12_c12_0
#define ICC_EOIR1	s3_0_c12_c12_1
#define ICC_HPPIR1	s3_0_c12_c12_2
#define ICC_BPR1	s3_0_c12_c12_3
#define ICC_CTLR	s3_0_c12_c12_4
#define ICC_SRE_EL1	s3_0_c12_c12_5
#define  ICC_SRE_EL1_EN		0x7
#define ICC_IGRPEN0	s3_0_c12_c12_6
#define ICC_IGRPEN1	s3_0_c12_c12_7

#define _STR(x) #x
#define STR(x) _STR(x)

/* distributor registers */
#define GICD_CTLR		0x0000
/* non-secure */
#define  GICD_CTLR_RWP			(1U << 31)
#define  GICD_CTLR_EnableGrp1		(1 << 0)
#define  GICD_CTLR_EnableGrp1A		(1 << 1)
#define  GICD_CTLR_ARE_NS		(1 << 4)
#define  GICD_CTLR_DS			(1 << 6)
#define GICD_TYPER		0x0004
#define  GICD_TYPER_MBIS		(1 << 16)
#define  GICD_TYPER_LPIS		(1 << 17)
#define  GICD_TYPER_ITLINE_M		0x1f
#define GICD_IIDR		0x0008
#define GICD_SETSPI_NSR		0x0040
#define GICD_CLRSPI_NSR		0x0048
#define GICD_IGROUPR(i)		(0x0080 + (IRQ_TO_REG32(i) * 4))
#define GICD_ISENABLER(i)	(0x0100 + (IRQ_TO_REG32(i) * 4))
#define GICD_ICENABLER(i)	(0x0180 + (IRQ_TO_REG32(i) * 4))
#define GICD_ISPENDR(i)		(0x0200 + (IRQ_TO_REG32(i) * 4))
#define GICD_ICPENDR(i)		(0x0280 + (IRQ_TO_REG32(i) * 4))
#define GICD_ISACTIVER(i)	(0x0300 + (IRQ_TO_REG32(i) * 4))
#define GICD_ICACTIVER(i)	(0x0380 + (IRQ_TO_REG32(i) * 4))
#define GICD_IPRIORITYR(i)	(0x0400 + (i))
#define GICD_ICFGR(i)		(0x0c00 + (IRQ_TO_REG16(i) * 4))
#define  GICD_ICFGR_TRIG_LEVEL(i)	(0x0 << (IRQ_TO_REG16BIT(i) * 2))
#define  GICD_ICFGR_TRIG_EDGE(i)	(0x2 << (IRQ_TO_REG16BIT(i) * 2))
#define  GICD_ICFGR_TRIG_MASK(i)	(0x2 << (IRQ_TO_REG16BIT(i) * 2))
#define GICD_IGRPMODR(i)	(0x0d00 + (IRQ_TO_REG32(i) * 4))
#define GICD_NSACR(i)		(0x0e00 + (IRQ_TO_REG16(i) * 4))
#define GICD_IROUTER(i)		(0x6000 + ((i) * 8))

/* redistributor registers */
#define GICR_CTLR		0x00000
#define  GICR_CTLR_RWP			((1U << 31) | (1 << 3))
#define  GICR_CTLR_ENABLE_LPIS		(1 << 0)
#define GICR_IIDR		0x00004
#define GICR_TYPER		0x00008
#define  GICR_TYPER_LAST		(1 << 4)
#define  GICR_TYPER_VLPIS		(1 << 1)
#define GICR_WAKER		0x00014
#define  GICR_WAKER_X31			(1U << 31)
#define  GICR_WAKER_CHILDRENASLEEP	(1 << 2)
#define  GICR_WAKER_PROCESSORSLEEP	(1 << 1)
#define  GICR_WAKER_X0			(1 << 0)
#define GICR_PROPBASER		0x00070
#define  GICR_PROPBASER_ISH		(1ULL << 10)
#define  GICR_PROPBASER_IC_NORM_NC	(1ULL << 7)
#define GICR_PENDBASER		0x00078
#define  GICR_PENDBASER_PTZ		(1ULL << 62)
#define  GICR_PENDBASER_ISH		(1ULL << 10)
#define  GICR_PENDBASER_IC_NORM_NC	(1ULL << 7)
#define GICR_IGROUPR0		0x10080
#define GICR_ISENABLE0		0x10100
#define GICR_ICENABLE0		0x10180
#define GICR_ISPENDR0		0x10200
#define GICR_ICPENDR0		0x10280
#define GICR_ISACTIVE0		0x10300
#define GICR_ICACTIVE0		0x10380
#define GICR_IPRIORITYR(i)	(0x10400 + (i))
#define GICR_ICFGR0		0x10c00
#define GICR_ICFGR1		0x10c04
#define GICR_IGRPMODR0		0x10d00

#define GICR_PROP_SIZE		(64 * 1024)
#define  GICR_PROP_GROUP1	(1 << 1)
#define  GICR_PROP_ENABLE	(1 << 0)
#define GICR_PEND_SIZE		(64 * 1024)

#define PPI_BASE		16
#define SPI_BASE		32
#define LPI_BASE		8192

#define IRQ_TO_REG32(i)		(((i) >> 5) & 0x1f)
#define IRQ_TO_REG32BIT(i)	((i) & 0x1f)

#define IRQ_TO_REG16(i)		(((i) >> 4) & 0x3f)
#define IRQ_TO_REG16BIT(i)	((i) & 0xf)

#define IRQ_ENABLE	1
#define IRQ_DISABLE	0

struct agintc_mbi_range {
	int			  mr_base;
	int			  mr_span;
	void			**mr_mbi;
};

struct agintc_lpi_info {
	struct agintc_msi_softc	*li_msic;
	struct cpu_info		*li_ci;
	uint32_t		 li_deviceid;
	uint32_t		 li_eventid;
	struct intrhand		*li_ih;
};

struct agintc_softc {
	struct simplebus_softc	 sc_sbus;
	struct intrq		*sc_handler;
	struct agintc_lpi_info	**sc_lpi;
	bus_space_tag_t		 sc_iot;
	bus_space_handle_t	 sc_d_ioh;
	bus_space_handle_t	*sc_r_ioh;
	bus_space_handle_t	 sc_redist_base;
	bus_dma_tag_t		 sc_dmat;
	uint16_t		*sc_processor;
	int			 sc_cpuremap[MAXCPUS];
	int			 sc_nintr;
	int			 sc_nlpi;
	bus_addr_t		 sc_mbi_addr;
	int			 sc_mbi_nranges;
	struct agintc_mbi_range	*sc_mbi_ranges;
	int			 sc_prio_shift;
	int			 sc_pmr_shift;
	int			 sc_rk3399_quirk;
	struct evcount		 sc_spur;
	int			 sc_ncells;
	int			 sc_num_redist;
	struct agintc_dmamem	*sc_prop;
	struct agintc_dmamem	*sc_pend;
	struct interrupt_controller sc_ic;
	int			 sc_ipi_num[3]; /* id for each ipi */
	int			 sc_ipi_reason[MAXCPUS]; /* cause of ipi */
	void			*sc_ipi_irq[3]; /* irqhandle for each ipi */
};
struct agintc_softc *agintc_sc;

struct intrhand {
	TAILQ_ENTRY(intrhand)	 ih_list;		/* link on intrq list */
	int			(*ih_func)(void *);	/* handler */
	void			*ih_arg;		/* arg for handler */
	int			 ih_ipl;		/* IPL_* */
	int			 ih_flags;
	int			 ih_irq;		/* IRQ number */
	struct evcount		 ih_count;
	char			*ih_name;
	struct cpu_info		*ih_ci;			/* CPU the IRQ runs on */
};

struct intrq {
	TAILQ_HEAD(, intrhand)	iq_list;	/* handler list */
	struct cpu_info		*iq_ci;		/* CPU the IRQ runs on */
	int			iq_irq_max;	/* IRQ to mask while handling */
	int			iq_irq_min;	/* lowest IRQ when shared */
	int			iq_ist;		/* share type */
	int			iq_route;
};

struct agintc_dmamem {
	bus_dmamap_t		adm_map;
	bus_dma_segment_t	adm_seg;
	size_t			adm_size;
	caddr_t			adm_kva;
};

#define AGINTC_DMA_MAP(_adm)	((_adm)->adm_map)
#define AGINTC_DMA_LEN(_adm)	((_adm)->adm_size)
#define AGINTC_DMA_DVA(_adm)	((_adm)->adm_map->dm_segs[0].ds_addr)
#define AGINTC_DMA_KVA(_adm)	((void *)(_adm)->adm_kva)

struct agintc_dmamem *agintc_dmamem_alloc(bus_dma_tag_t, bus_size_t,
		    bus_size_t);
void		agintc_dmamem_free(bus_dma_tag_t, struct agintc_dmamem *);

int		agintc_match(struct device *, void *, void *);
void		agintc_attach(struct device *, struct device *, void *);
void		agintc_mbiinit(struct agintc_softc *, int, bus_addr_t);
void		agintc_cpuinit(void);
int		agintc_spllower(int);
void		agintc_splx(int);
int		agintc_splraise(int);
void		agintc_setipl(int);
void		agintc_enable_wakeup(void);
void		agintc_disable_wakeup(void);
void		agintc_calc_mask(void);
void		agintc_calc_irq(struct agintc_softc *sc, int irq);
void		*agintc_intr_establish(int, int, int, struct cpu_info *,
		    int (*)(void *), void *, char *);
void		*agintc_intr_establish_fdt(void *cookie, int *cell, int level,
		    struct cpu_info *, int (*func)(void *), void *arg, char *name);
void		*agintc_intr_establish_mbi(void *, uint64_t *, uint64_t *,
		    int , struct cpu_info *, int (*)(void *), void *, char *);
void		agintc_intr_disestablish(void *);
void		agintc_intr_set_wakeup(void *);
void		agintc_irq_handler(void *);
uint32_t	agintc_iack(void);
void		agintc_eoi(uint32_t);
void		agintc_set_priority(struct agintc_softc *sc, int, int);
void		agintc_intr_enable(struct agintc_softc *, int);
void		agintc_intr_disable(struct agintc_softc *, int);
void		agintc_intr_config(struct agintc_softc *, int, int);
void		agintc_route(struct agintc_softc *, int, int,
		    struct cpu_info *);
void		agintc_route_irq(void *, int, struct cpu_info *);
void		agintc_intr_barrier(void *);
void		agintc_wait_rwp(struct agintc_softc *sc);
void		agintc_r_wait_rwp(struct agintc_softc *sc);
uint32_t	agintc_r_ictlr(void);

int		agintc_ipi_ddb(void *v);
int		agintc_ipi_halt(void *v);
int		agintc_ipi_nop(void *v);
int		agintc_ipi_combined(void *);
void		agintc_send_ipi(struct cpu_info *, int);

void		agintc_msi_discard(struct agintc_lpi_info *);
void		agintc_msi_inv(struct agintc_lpi_info *);

const struct cfattach	agintc_ca = {
	sizeof (struct agintc_softc), agintc_match, agintc_attach
};

struct cfdriver agintc_cd = {
	NULL, "agintc", DV_DULL
};

static char *agintc_compatibles[] = {
	"arm,gic-v3",
	"arm,gic-v4",
	NULL
};

int
agintc_match(struct device *parent, void *cfdata, void *aux)
{
	struct fdt_attach_args *faa = aux;
	int i;

	for (i = 0; agintc_compatibles[i]; i++)
		if (OF_is_compatible(faa->fa_node, agintc_compatibles[i]))
			return (1);

	return (0);
}

static void
__isb(void)
{
	__asm volatile("isb");
}

void
agintc_attach(struct device *parent, struct device *self, void *aux)
{
	struct agintc_softc	*sc = (struct agintc_softc *)self;
	struct fdt_attach_args	*faa = aux;
	struct cpu_info		*ci;
	CPU_INFO_ITERATOR	 cii;
	u_long			 psw;
	uint32_t		 typer;
	uint32_t		 nsacr, oldnsacr;
	uint32_t		 pmr, oldpmr;
	uint32_t		 ctrl, bits;
	uint32_t		 affinity;
	int			 i, nbits, nintr;
	int			 offset, nredist;
#ifdef MULTIPROCESSOR
	int			 nipi, ipiirq[3];
#endif

	psw = intr_disable();
	arm_init_smask();

	sc->sc_iot = faa->fa_iot;
	sc->sc_dmat = faa->fa_dmat;

	/* First row: distributor */
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_d_ioh))
		panic("%s: ICD bus_space_map failed!", __func__);

	/* Second row: redistributor */
	if (bus_space_map(sc->sc_iot, faa->fa_reg[1].addr,
	    faa->fa_reg[1].size, 0, &sc->sc_redist_base))
		panic("%s: ICP bus_space_map failed!", __func__);

	typer = bus_space_read_4(sc->sc_iot, sc->sc_d_ioh, GICD_TYPER);

	if (typer & GICD_TYPER_LPIS) {
		/* Allocate redistributor tables */
		sc->sc_prop = agintc_dmamem_alloc(sc->sc_dmat,
		    GICR_PROP_SIZE, GICR_PROP_SIZE);
		if (sc->sc_prop == NULL) {
			printf(": can't alloc LPI config table\n");
			goto unmap;
		}
		sc->sc_pend = agintc_dmamem_alloc(sc->sc_dmat,
		    GICR_PEND_SIZE, GICR_PEND_SIZE);
		if (sc->sc_pend == NULL) {
			printf(": can't alloc LPI pending table\n");
			goto unmap;
		}

		/* Minimum number of LPIs supported by any implementation. */
		sc->sc_nlpi = 8192;
	}

	if (typer & GICD_TYPER_MBIS)
		agintc_mbiinit(sc, faa->fa_node, faa->fa_reg[0].addr);

	/*
	 * We are guaranteed to have at least 16 priority levels, so
	 * in principle we just want to use the top 4 bits of the
	 * (non-secure) priority field.
	 */
	sc->sc_prio_shift = sc->sc_pmr_shift = 4;

	/*
	 * If the system supports two security states and SCR_EL3.FIQ
	 * is zero, the non-secure shifted view applies.  We detect
	 * this by checking whether the number of writable bits
	 * matches the number of implemented priority bits.  If that
	 * is the case we will need to adjust the priorities that we
	 * write into ICC_PMR_EL1 accordingly.
	 *
	 * On Ampere eMAG it appears as if there are five writable
	 * bits when we write 0xff.  But for higher priorities
	 * (smaller values) only the top 4 bits stick.  So we use 0xbf
	 * instead to determine the number of writable bits.
	 */
	ctrl = bus_space_read_4(sc->sc_iot, sc->sc_d_ioh, GICD_CTLR);
	if ((ctrl & GICD_CTLR_DS) == 0) {
		__asm volatile("mrs %x0, "STR(ICC_CTLR_EL1) : "=r"(ctrl));
		nbits = ICC_CTLR_EL1_PRIBITS(ctrl) + 1;
		__asm volatile("mrs %x0, "STR(ICC_PMR) : "=r"(oldpmr));
		__asm volatile("msr "STR(ICC_PMR)", %x0" :: "r"(0xbf));
		__asm volatile("mrs %x0, "STR(ICC_PMR) : "=r"(pmr));
		__asm volatile("msr "STR(ICC_PMR)", %x0" :: "r"(oldpmr));
		if (nbits == 8 - (ffs(pmr) - 1))
			sc->sc_pmr_shift--;
	}

	/*
	 * The Rockchip RK3399 is busted.  Its GIC-500 treats all
	 * access to its memory mapped registers as "secure".  As a
	 * result, several registers don't behave as expected.  For
	 * example, the GICD_IPRIORITYRn and GICR_IPRIORITYRn
	 * registers expose the full priority range available to
	 * secure interrupts.  We need to be aware of this and write
	 * an adjusted priority value into these registers.  We also
	 * need to be careful not to touch any bits that shouldn't be
	 * writable in non-secure mode.
	 *
	 * We check whether we have secure mode access to these
	 * registers by attempting to write to the GICD_NSACR register
	 * and check whether its contents actually change.  In that
	 * case we need to adjust the priorities we write into
	 * GICD_IPRIORITYRn and GICRIPRIORITYRn accordingly.
	 */
	oldnsacr = bus_space_read_4(sc->sc_iot, sc->sc_d_ioh, GICD_NSACR(32));
	bus_space_write_4(sc->sc_iot, sc->sc_d_ioh, GICD_NSACR(32),
	    oldnsacr ^ 0xffffffff);
	nsacr = bus_space_read_4(sc->sc_iot, sc->sc_d_ioh, GICD_NSACR(32));
	if (nsacr != oldnsacr) {
		bus_space_write_4(sc->sc_iot, sc->sc_d_ioh, GICD_NSACR(32),
		    oldnsacr);
		sc->sc_rk3399_quirk = 1;
		sc->sc_prio_shift--;
		printf(" sec");
	}

	printf(" shift %d:%d", sc->sc_prio_shift, sc->sc_pmr_shift);

	evcount_attach(&sc->sc_spur, "irq1023/spur", NULL);

	__asm volatile("msr "STR(ICC_SRE_EL1)", %x0" : : "r" (ICC_SRE_EL1_EN));
	__isb();

	nintr = 32 * (typer & GICD_TYPER_ITLINE_M);
	nintr += 32; /* ICD_ICTR + 1, irq 0-31 is SGI, 32+ is PPI */
	sc->sc_nintr = nintr;

	agintc_sc = sc; /* save this for global access */

	/* find the redistributors. */
	offset = 0;
	for (nredist = 0; ; nredist++) {
		int32_t sz = (64 * 1024 * 2);
		uint64_t typer;

		typer = bus_space_read_8(sc->sc_iot, sc->sc_redist_base,
		    offset + GICR_TYPER);

		if (typer & GICR_TYPER_VLPIS)
			sz += (64 * 1024 * 2);

#ifdef DEBUG_AGINTC
		printf("probing redistributor %d %x\n", nredist, offset);
#endif

		offset += sz;

		if (typer & GICR_TYPER_LAST) {
			sc->sc_num_redist = nredist + 1;
			break;
		}
	}

	printf(" nirq %d nredist %d", nintr, sc->sc_num_redist);
	
	sc->sc_r_ioh = mallocarray(sc->sc_num_redist,
	    sizeof(*sc->sc_r_ioh), M_DEVBUF, M_WAITOK);
	sc->sc_processor = mallocarray(sc->sc_num_redist,
	    sizeof(*sc->sc_processor), M_DEVBUF, M_WAITOK);

	/* submap and configure the redistributors. */
	offset = 0;
	for (nredist = 0; nredist < sc->sc_num_redist; nredist++) {
		int32_t sz = (64 * 1024 * 2);
		uint64_t typer;

		typer = bus_space_read_8(sc->sc_iot, sc->sc_redist_base,
		    offset + GICR_TYPER);

		if (typer & GICR_TYPER_VLPIS)
			sz += (64 * 1024 * 2);

		affinity = bus_space_read_8(sc->sc_iot,
		    sc->sc_redist_base, offset + GICR_TYPER) >> 32;
		CPU_INFO_FOREACH(cii, ci) {
			if (affinity == (((ci->ci_mpidr >> 8) & 0xff000000) |
			    (ci->ci_mpidr & 0x00ffffff)))
				break;
		}
		if (ci != NULL)
			sc->sc_cpuremap[ci->ci_cpuid] = nredist;

		sc->sc_processor[nredist] = bus_space_read_8(sc->sc_iot,
		    sc->sc_redist_base, offset + GICR_TYPER) >> 8;

		bus_space_subregion(sc->sc_iot, sc->sc_redist_base,
		    offset, sz, &sc->sc_r_ioh[nredist]);

		if (sc->sc_nlpi > 0) {
			bus_space_write_8(sc->sc_iot, sc->sc_redist_base,
			    offset + GICR_PROPBASER,
			    AGINTC_DMA_DVA(sc->sc_prop) |
			    GICR_PROPBASER_ISH | GICR_PROPBASER_IC_NORM_NC |
			    fls(LPI_BASE + sc->sc_nlpi - 1) - 1);
			bus_space_write_8(sc->sc_iot, sc->sc_redist_base,
			    offset + GICR_PENDBASER,
			    AGINTC_DMA_DVA(sc->sc_pend) |
			    GICR_PENDBASER_ISH | GICR_PENDBASER_IC_NORM_NC |
			    GICR_PENDBASER_PTZ);
			bus_space_write_4(sc->sc_iot, sc->sc_redist_base,
			    offset + GICR_CTLR, GICR_CTLR_ENABLE_LPIS);
		}

		offset += sz;
	}

	/* Disable all interrupts, clear all pending */
	for (i = 1; i < nintr / 32; i++) {
		bus_space_write_4(sc->sc_iot, sc->sc_d_ioh,
		    GICD_ICACTIVER(i * 32), ~0);
		bus_space_write_4(sc->sc_iot, sc->sc_d_ioh,
		    GICD_ICENABLER(i * 32), ~0);
	}

	for (i = 4; i < nintr; i += 4) {
		/* lowest priority ?? */
		bus_space_write_4(sc->sc_iot, sc->sc_d_ioh,
		    GICD_IPRIORITYR(i), 0xffffffff);
	}

	/* Set all interrupts to G1NS */
	for (i = 1; i < nintr / 32; i++) {
		bus_space_write_4(sc->sc_iot, sc->sc_d_ioh,
		    GICD_IGROUPR(i * 32), ~0);
		bus_space_write_4(sc->sc_iot, sc->sc_d_ioh,
		    GICD_IGRPMODR(i * 32), 0);
	}

	for (i = 2; i < nintr / 16; i++) {
		/* irq 32 - N */
		bus_space_write_4(sc->sc_iot, sc->sc_d_ioh,
		    GICD_ICFGR(i * 16), 0);
	}

	agintc_cpuinit();

	sc->sc_handler = mallocarray(nintr,
	    sizeof(*sc->sc_handler), M_DEVBUF, M_ZERO | M_WAITOK);
	for (i = 0; i < nintr; i++)
		TAILQ_INIT(&sc->sc_handler[i].iq_list);
	sc->sc_lpi = mallocarray(sc->sc_nlpi,
	    sizeof(*sc->sc_lpi), M_DEVBUF, M_ZERO | M_WAITOK);

	/* set priority to IPL_HIGH until configure lowers to desired IPL */
	agintc_setipl(IPL_HIGH);

	/* initialize all interrupts as disabled */
	agintc_calc_mask();

	/* insert self as interrupt handler */
	arm_set_intr_handler(agintc_splraise, agintc_spllower, agintc_splx,
	    agintc_setipl, agintc_irq_handler, NULL,
	    agintc_enable_wakeup, agintc_disable_wakeup);

	/* enable interrupts */
	ctrl = bus_space_read_4(sc->sc_iot, sc->sc_d_ioh, GICD_CTLR);
	bits = GICD_CTLR_ARE_NS | GICD_CTLR_EnableGrp1A | GICD_CTLR_EnableGrp1;
	if (sc->sc_rk3399_quirk) {
		bits &= ~GICD_CTLR_EnableGrp1A;
		bits <<= 1;
	}
	bus_space_write_4(sc->sc_iot, sc->sc_d_ioh, GICD_CTLR, ctrl | bits);

	__asm volatile("msr "STR(ICC_PMR)", %x0" :: "r"(0xff));
	__asm volatile("msr "STR(ICC_BPR1)", %x0" :: "r"(0));
	__asm volatile("msr "STR(ICC_IGRPEN1)", %x0" :: "r"(1));

#ifdef MULTIPROCESSOR
	/* setup IPI interrupts */

	/*
	 * Ideally we want three IPI interrupts, one for NOP, one for
	 * DDB and one for HALT.  However we can survive if only one
	 * is available; it is possible that most are not available to
	 * the non-secure OS.
	 */
	nipi = 0;
	for (i = 0; i < 16; i++) {
		int hwcpu = sc->sc_cpuremap[cpu_number()];
		int reg, oldreg;

		oldreg = bus_space_read_1(sc->sc_iot, sc->sc_r_ioh[hwcpu],
		    GICR_IPRIORITYR(i));
		bus_space_write_1(sc->sc_iot, sc->sc_r_ioh[hwcpu],
		    GICR_IPRIORITYR(i), oldreg ^ 0x20);

		/* if this interrupt is not usable, pri will be unmodified */
		reg = bus_space_read_1(sc->sc_iot, sc->sc_r_ioh[hwcpu],
		    GICR_IPRIORITYR(i));
		if (reg == oldreg)
			continue;

		/* return to original value, will be set when used */
		bus_space_write_1(sc->sc_iot, sc->sc_r_ioh[hwcpu],
		    GICR_IPRIORITYR(i), oldreg);

		if (nipi == 0)
			printf(" ipi: %d", i);
		else
			printf(", %d", i);
		ipiirq[nipi++] = i;
		if (nipi == 3)
			break;
	}

	if (nipi == 0)
		panic("no irq available for IPI");

	switch (nipi) {
	case 1:
		sc->sc_ipi_irq[0] = agintc_intr_establish(ipiirq[0],
		    IST_EDGE_RISING, IPL_IPI|IPL_MPSAFE, NULL,
		    agintc_ipi_combined, sc, "ipi");
		sc->sc_ipi_num[ARM_IPI_NOP] = ipiirq[0];
		sc->sc_ipi_num[ARM_IPI_DDB] = ipiirq[0];
		sc->sc_ipi_num[ARM_IPI_HALT] = ipiirq[0];
		break;
	case 2:
		sc->sc_ipi_irq[0] = agintc_intr_establish(ipiirq[0],
		    IST_EDGE_RISING, IPL_IPI|IPL_MPSAFE, NULL,
		    agintc_ipi_nop, sc, "ipinop");
		sc->sc_ipi_num[ARM_IPI_NOP] = ipiirq[0];
		sc->sc_ipi_irq[1] = agintc_intr_establish(ipiirq[1],
		    IST_EDGE_RISING, IPL_IPI|IPL_MPSAFE, NULL,
		    agintc_ipi_combined, sc, "ipi");
		sc->sc_ipi_num[ARM_IPI_DDB] = ipiirq[1];
		sc->sc_ipi_num[ARM_IPI_HALT] = ipiirq[1];
		break;
	case 3:
		sc->sc_ipi_irq[0] = agintc_intr_establish(ipiirq[0],
		    IST_EDGE_RISING, IPL_IPI|IPL_MPSAFE, NULL,
		    agintc_ipi_nop, sc, "ipinop");
		sc->sc_ipi_num[ARM_IPI_NOP] = ipiirq[0];
		sc->sc_ipi_irq[1] = agintc_intr_establish(ipiirq[1],
		    IST_EDGE_RISING, IPL_IPI|IPL_MPSAFE, NULL,
		    agintc_ipi_ddb, sc, "ipiddb");
		sc->sc_ipi_num[ARM_IPI_DDB] = ipiirq[1];
		sc->sc_ipi_irq[2] = agintc_intr_establish(ipiirq[2],
		    IST_EDGE_RISING, IPL_IPI|IPL_MPSAFE, NULL,
		    agintc_ipi_halt, sc, "ipihalt");
		sc->sc_ipi_num[ARM_IPI_HALT] = ipiirq[2];
		break;
	default:
		panic("nipi unexpected number %d", nipi);
	}

	intr_send_ipi_func = agintc_send_ipi;
#endif

	sc->sc_ic.ic_node = faa->fa_node;
	sc->sc_ic.ic_cookie = self;
	sc->sc_ic.ic_establish = agintc_intr_establish_fdt;
	sc->sc_ic.ic_disestablish = agintc_intr_disestablish;
	sc->sc_ic.ic_route = agintc_route_irq;
	sc->sc_ic.ic_cpu_enable = agintc_cpuinit;
	sc->sc_ic.ic_barrier = agintc_intr_barrier;
	if (sc->sc_mbi_nranges > 0)
		sc->sc_ic.ic_establish_msi = agintc_intr_establish_mbi;
	sc->sc_ic.ic_set_wakeup = agintc_intr_set_wakeup;
	arm_intr_register_fdt(&sc->sc_ic);

	intr_restore(psw);

	/* Attach ITS. */
	simplebus_attach(parent, &sc->sc_sbus.sc_dev, faa);

	return;

unmap:
	if (sc->sc_r_ioh) {
		free(sc->sc_r_ioh, M_DEVBUF,
		    sc->sc_num_redist * sizeof(*sc->sc_r_ioh));
	}
	if (sc->sc_processor) {
		free(sc->sc_processor, M_DEVBUF,
		     sc->sc_num_redist * sizeof(*sc->sc_processor));
	}

	if (sc->sc_pend)
		agintc_dmamem_free(sc->sc_dmat, sc->sc_pend);
	if (sc->sc_prop)
		agintc_dmamem_free(sc->sc_dmat, sc->sc_prop);

	bus_space_unmap(sc->sc_iot, sc->sc_redist_base, faa->fa_reg[1].size);
	bus_space_unmap(sc->sc_iot, sc->sc_d_ioh, faa->fa_reg[0].size);
}

void
agintc_mbiinit(struct agintc_softc *sc, int node, bus_addr_t addr)
{
	uint32_t *ranges;
	int i, len;

	if (OF_getproplen(node, "msi-controller") != 0)
		return;

	len = OF_getproplen(node, "mbi-ranges");
	if (len <= 0 || len % 2 * sizeof(uint32_t) != 0)
		return;

	ranges = malloc(len, M_TEMP, M_WAITOK);
	OF_getpropintarray(node, "mbi-ranges", ranges, len);

	sc->sc_mbi_nranges = len / (2 * sizeof(uint32_t));
	sc->sc_mbi_ranges = mallocarray(sc->sc_mbi_nranges,
	    sizeof(struct agintc_mbi_range), M_DEVBUF, M_WAITOK);

	for (i = 0; i < sc->sc_mbi_nranges; i++) {
		sc->sc_mbi_ranges[i].mr_base = ranges[2 * i + 0];
		sc->sc_mbi_ranges[i].mr_span = ranges[2 * i + 1];
		sc->sc_mbi_ranges[i].mr_mbi =
		    mallocarray(sc->sc_mbi_ranges[i].mr_span,
			sizeof(void *), M_DEVBUF, M_WAITOK | M_ZERO);
	}

	free(ranges, M_TEMP, len);

	addr = OF_getpropint64(node, "mbi-alias", addr);
	sc->sc_mbi_addr = addr + GICD_SETSPI_NSR;

	printf(" mbi");
}

/* Initialize redistributors on each core. */
void
agintc_cpuinit(void)
{
	struct agintc_softc *sc = agintc_sc;
	uint32_t waker;
	int timeout = 100000;
	int hwcpu;
	int i;

	hwcpu = sc->sc_cpuremap[cpu_number()];
	waker = bus_space_read_4(sc->sc_iot, sc->sc_r_ioh[hwcpu],
	    GICR_WAKER);
	waker &= ~(GICR_WAKER_PROCESSORSLEEP);
	bus_space_write_4(sc->sc_iot, sc->sc_r_ioh[hwcpu], GICR_WAKER,
	    waker);

	do {
		waker = bus_space_read_4(sc->sc_iot, sc->sc_r_ioh[hwcpu],
		    GICR_WAKER);
	} while (--timeout && (waker & GICR_WAKER_CHILDRENASLEEP));
	if (timeout == 0)
		printf("%s: waker timed out\n", __func__);

	bus_space_write_4(sc->sc_iot, sc->sc_r_ioh[hwcpu],
	    GICR_ICENABLE0, ~0);
	bus_space_write_4(sc->sc_iot, sc->sc_r_ioh[hwcpu],
	    GICR_ICPENDR0, ~0);
	bus_space_write_4(sc->sc_iot, sc->sc_r_ioh[hwcpu],
	    GICR_ICACTIVE0, ~0);
	for (i = 0; i < 32; i += 4) {
		bus_space_write_4(sc->sc_iot, sc->sc_r_ioh[hwcpu],
		    GICR_IPRIORITYR(i), ~0);
	}
	bus_space_write_4(sc->sc_iot, sc->sc_r_ioh[hwcpu],
	    GICR_IGROUPR0, ~0);
	bus_space_write_4(sc->sc_iot, sc->sc_r_ioh[hwcpu],
	    GICR_IGRPMODR0, 0);

	if (sc->sc_ipi_irq[0] != NULL)
		agintc_route_irq(sc->sc_ipi_irq[0], IRQ_ENABLE, curcpu());
	if (sc->sc_ipi_irq[1] != NULL)
		agintc_route_irq(sc->sc_ipi_irq[1], IRQ_ENABLE, curcpu());
	if (sc->sc_ipi_irq[2] != NULL)
		agintc_route_irq(sc->sc_ipi_irq[2], IRQ_ENABLE, curcpu());

	__asm volatile("msr "STR(ICC_PMR)", %x0" :: "r"(0xff));
	__asm volatile("msr "STR(ICC_BPR1)", %x0" :: "r"(0));
	__asm volatile("msr "STR(ICC_IGRPEN1)", %x0" :: "r"(1));
	intr_enable();
}

void
agintc_set_priority(struct agintc_softc *sc, int irq, int ipl)
{
	struct cpu_info	*ci = curcpu();
	int		 hwcpu = sc->sc_cpuremap[ci->ci_cpuid];
	uint32_t	 prival;

	prival = ((0xff - ipl) << sc->sc_prio_shift) & 0xff;

	if (irq >= SPI_BASE) {
		bus_space_write_1(sc->sc_iot, sc->sc_d_ioh,
		    GICD_IPRIORITYR(irq), prival);
	} else  {
		/* only sets local redistributor */
		bus_space_write_1(sc->sc_iot, sc->sc_r_ioh[hwcpu],
		    GICR_IPRIORITYR(irq), prival);
	}
}

void
agintc_setipl(int ipl)
{
	struct agintc_softc	*sc = agintc_sc;
	struct cpu_info		*ci = curcpu();
	u_long			 psw;
	uint32_t		 prival;

	/* disable here is only to keep hardware in sync with ci->ci_cpl */
	psw = intr_disable();
	ci->ci_cpl = ipl;

	prival = ((0xff - ipl) << sc->sc_pmr_shift) & 0xff;
	__asm volatile("msr "STR(ICC_PMR)", %x0" : : "r" (prival));
	__isb();

	intr_restore(psw);
}

void
agintc_enable_wakeup(void)
{
	struct agintc_softc *sc = agintc_sc;
	struct intrhand *ih;
	uint8_t *prop;
	int irq, wakeup;

	for (irq = 0; irq < sc->sc_nintr; irq++) {
		/* No handler? Disabled already. */
		if (TAILQ_EMPTY(&sc->sc_handler[irq].iq_list))
			continue;
		/* Unless we're WAKEUP, disable. */
		wakeup = 0;
		TAILQ_FOREACH(ih, &sc->sc_handler[irq].iq_list, ih_list) {
			if (ih->ih_flags & IPL_WAKEUP) {
				wakeup = 1;
				break;
			}
		}
		if (!wakeup)
			agintc_intr_disable(sc, irq);
	}

	for (irq = 0; irq < sc->sc_nlpi; irq++) {
		if (sc->sc_lpi[irq] == NULL)
			continue;
		ih = sc->sc_lpi[irq]->li_ih;
		KASSERT(ih != NULL);
		if (ih->ih_flags & IPL_WAKEUP)
			continue;
		prop = AGINTC_DMA_KVA(sc->sc_prop);
		prop[irq] &= ~GICR_PROP_ENABLE;
		/* Make globally visible. */
		cpu_dcache_wb_range((vaddr_t)&prop[irq],
		    sizeof(*prop));
		__asm volatile("dsb sy");
		/* Invalidate cache */
		agintc_msi_inv(sc->sc_lpi[irq]);
	}
}

void
agintc_disable_wakeup(void)
{
	struct agintc_softc *sc = agintc_sc;
	struct intrhand *ih;
	uint8_t *prop;
	int irq, wakeup;

	for (irq = 0; irq < sc->sc_nintr; irq++) {
		/* No handler? Keep disabled. */
		if (TAILQ_EMPTY(&sc->sc_handler[irq].iq_list))
			continue;
		/* WAKEUPs are already enabled. */
		wakeup = 0;
		TAILQ_FOREACH(ih, &sc->sc_handler[irq].iq_list, ih_list) {
			if (ih->ih_flags & IPL_WAKEUP) {
				wakeup = 1;
				break;
			}
		}
		if (!wakeup)
			agintc_intr_enable(sc, irq);
	}

	for (irq = 0; irq < sc->sc_nlpi; irq++) {
		if (sc->sc_lpi[irq] == NULL)
			continue;
		ih = sc->sc_lpi[irq]->li_ih;
		KASSERT(ih != NULL);
		if (ih->ih_flags & IPL_WAKEUP)
			continue;
		prop = AGINTC_DMA_KVA(sc->sc_prop);
		prop[irq] |= GICR_PROP_ENABLE;
		/* Make globally visible. */
		cpu_dcache_wb_range((vaddr_t)&prop[irq],
		    sizeof(*prop));
		__asm volatile("dsb sy");
		/* Invalidate cache */
		agintc_msi_inv(sc->sc_lpi[irq]);
	}
}

void
agintc_intr_enable(struct agintc_softc *sc, int irq)
{
	struct cpu_info	*ci = curcpu();
	int hwcpu = sc->sc_cpuremap[ci->ci_cpuid];
	int bit = 1 << IRQ_TO_REG32BIT(irq);

	if (irq >= 32) {
		bus_space_write_4(sc->sc_iot, sc->sc_d_ioh,
		    GICD_ISENABLER(irq), bit);
	} else {
		bus_space_write_4(sc->sc_iot, sc->sc_r_ioh[hwcpu],
		    GICR_ISENABLE0, bit);
	}
}

void
agintc_intr_disable(struct agintc_softc *sc, int irq)
{
	struct cpu_info	*ci = curcpu();
	int hwcpu = sc->sc_cpuremap[ci->ci_cpuid];

	if (irq >= 32) {
		bus_space_write_4(sc->sc_iot, sc->sc_d_ioh,
		    GICD_ICENABLER(irq), 1 << IRQ_TO_REG32BIT(irq));
	} else {
		bus_space_write_4(sc->sc_iot, sc->sc_r_ioh[hwcpu],
		    GICR_ICENABLE0, 1 << IRQ_TO_REG32BIT(irq));
	}
}

void
agintc_intr_config(struct agintc_softc *sc, int irq, int type)
{
	uint32_t reg;

	/* Don't dare to change SGIs or PPIs (yet) */
	if (irq < 32)
		return;

	reg = bus_space_read_4(sc->sc_iot, sc->sc_d_ioh, GICD_ICFGR(irq));
	reg &= ~GICD_ICFGR_TRIG_MASK(irq);
	if (type == IST_EDGE_RISING)
		reg |= GICD_ICFGR_TRIG_EDGE(irq);
	else
		reg |= GICD_ICFGR_TRIG_LEVEL(irq);
	bus_space_write_4(sc->sc_iot, sc->sc_d_ioh, GICD_ICFGR(irq), reg);
}

void
agintc_calc_mask(void)
{
	struct agintc_softc	*sc = agintc_sc;
	int			 irq;

	for (irq = 0; irq < sc->sc_nintr; irq++)
		agintc_calc_irq(sc, irq);
}

void
agintc_calc_irq(struct agintc_softc *sc, int irq)
{
	struct cpu_info	*ci = sc->sc_handler[irq].iq_ci;
	struct intrhand	*ih;
	int max = IPL_NONE;
	int min = IPL_HIGH;
	
	TAILQ_FOREACH(ih, &sc->sc_handler[irq].iq_list, ih_list) {
		if (ih->ih_ipl > max)
			max = ih->ih_ipl;

		if (ih->ih_ipl < min)
			min = ih->ih_ipl;
	}

	if (max == IPL_NONE)
		min = IPL_NONE;

	if (sc->sc_handler[irq].iq_irq_max == max &&
	    sc->sc_handler[irq].iq_irq_min == min)
		return;

	sc->sc_handler[irq].iq_irq_max = max;
	sc->sc_handler[irq].iq_irq_min = min;

#ifdef DEBUG_AGINTC
	if (min != IPL_NONE)
		printf("irq %d to block at %d %d \n", irq, max, min );
#endif
	/* Enable interrupts at lower levels, clear -> enable */
	/* Set interrupt priority/enable */
	if (min != IPL_NONE) {
		agintc_set_priority(sc, irq, min);
		agintc_route(sc, irq, IRQ_ENABLE, ci);
		agintc_intr_enable(sc, irq);
	} else {
		agintc_intr_disable(sc, irq);
		agintc_route(sc, irq, IRQ_DISABLE, ci);
	}
}

void
agintc_splx(int new)
{
	struct cpu_info *ci = curcpu();

	if (ci->ci_ipending & arm_smask[new])
		arm_do_pending_intr(new);

	agintc_setipl(new);
}

int
agintc_spllower(int new)
{
	struct cpu_info *ci = curcpu();
	int old = ci->ci_cpl;

	agintc_splx(new);
	return (old);
}

int
agintc_splraise(int new)
{
	struct cpu_info	*ci = curcpu();
	int old = ci->ci_cpl;

	/*
	 * setipl must always be called because there is a race window
	 * where the variable is updated before the mask is set
	 * an interrupt occurs in that window without the mask always
	 * being set, the hardware might not get updated on the next
	 * splraise completely messing up spl protection.
	 */
	if (old > new)
		new = old;

	agintc_setipl(new);
	return (old);
}

uint32_t
agintc_iack(void)
{
	int irq;

	__asm volatile("mrs %x0, "STR(ICC_IAR1) : "=r" (irq));
	__asm volatile("dsb sy");
	return irq;
}

void
agintc_route_irq(void *v, int enable, struct cpu_info *ci)
{
	struct agintc_softc	*sc = agintc_sc;
	struct intrhand		*ih = v;

	if (enable) {
		agintc_set_priority(sc, ih->ih_irq,
		    sc->sc_handler[ih->ih_irq].iq_irq_min);
		agintc_route(sc, ih->ih_irq, IRQ_ENABLE, ci);
		agintc_intr_enable(sc, ih->ih_irq);
	}
}

void
agintc_route(struct agintc_softc *sc, int irq, int enable, struct cpu_info *ci)
{
	/* XXX does not yet support 'participating node' */
	if (irq >= 32) {
#ifdef DEBUG_AGINTC
		printf("router %x irq %d val %016llx\n", GICD_IROUTER(irq),
		    irq, ci->ci_mpidr & MPIDR_AFF);
#endif
		bus_space_write_8(sc->sc_iot, sc->sc_d_ioh,
		    GICD_IROUTER(irq), ci->ci_mpidr & MPIDR_AFF);
	}
}

void
agintc_intr_barrier(void *cookie)
{
	struct intrhand		*ih = cookie;

	sched_barrier(ih->ih_ci);
}

void
agintc_run_handler(struct intrhand *ih, void *frame, int s)
{
	void *arg;
	int handled;

#ifdef MULTIPROCESSOR
	int need_lock;

	if (ih->ih_flags & IPL_MPSAFE)
		need_lock = 0;
	else
		need_lock = s < IPL_SCHED;

	if (need_lock)
		KERNEL_LOCK();
#endif

	if (ih->ih_arg)
		arg = ih->ih_arg;
	else
		arg = frame;

	handled = ih->ih_func(arg);
	if (handled)
		ih->ih_count.ec_count++;

#ifdef MULTIPROCESSOR
	if (need_lock)
		KERNEL_UNLOCK();
#endif
}

void
agintc_irq_handler(void *frame)
{
	struct agintc_softc	*sc = agintc_sc;
	struct intrhand		*ih;
	int			 irq, pri, s;

	irq = agintc_iack();

#ifdef DEBUG_AGINTC
	if (irq != 30)
		printf("irq  %d fired\n", irq);
	else {
		static int cnt = 0;
		if ((cnt++ % 100) == 0) {
			printf("irq  %d fired * _100\n", irq);
#ifdef DDB
			db_enter();
#endif
		}
	}
#endif

	if (irq == 1023) {
		sc->sc_spur.ec_count++;
		return;
	}

	if ((irq >= sc->sc_nintr && irq < LPI_BASE) ||
	    irq >= LPI_BASE + sc->sc_nlpi) {
		return;
	}

	if (irq >= LPI_BASE) {
		if (sc->sc_lpi[irq - LPI_BASE] == NULL)
			return;
		ih = sc->sc_lpi[irq - LPI_BASE]->li_ih;
		KASSERT(ih != NULL);

		s = agintc_splraise(ih->ih_ipl);
		intr_enable();
		agintc_run_handler(ih, frame, s);
		intr_disable();
		agintc_eoi(irq);

		agintc_splx(s);
		return;
	}

	pri = sc->sc_handler[irq].iq_irq_max;
	s = agintc_splraise(pri);
	intr_enable();
	TAILQ_FOREACH(ih, &sc->sc_handler[irq].iq_list, ih_list) {
		agintc_run_handler(ih, frame, s);
	}
	intr_disable();
	agintc_eoi(irq);

	agintc_splx(s);
}

void *
agintc_intr_establish_fdt(void *cookie, int *cell, int level,
    struct cpu_info *ci, int (*func)(void *), void *arg, char *name)
{
	struct agintc_softc	*sc = agintc_sc;
	int			 irq;
	int			 type;

	/* 2nd cell contains the interrupt number */
	irq = cell[1];

	/* 1st cell contains type: 0 SPI (32-X), 1 PPI (16-31) */
	if (cell[0] == 0)
		irq += SPI_BASE;
	else if (cell[0] == 1)
		irq += PPI_BASE;
	else
		panic("%s: bogus interrupt type", sc->sc_sbus.sc_dev.dv_xname);

	/* SPIs are only active-high level or low-to-high edge */
	if (cell[2] & 0x3)
		type = IST_EDGE_RISING;
	else
		type = IST_LEVEL_HIGH;

	return agintc_intr_establish(irq, type, level, ci, func, arg, name);
}

void *
agintc_intr_establish(int irqno, int type, int level, struct cpu_info *ci,
    int (*func)(void *), void *arg, char *name)
{
	struct agintc_softc	*sc = agintc_sc;
	struct intrhand		*ih;
	u_long			 psw;

	if (irqno < 0 || (irqno >= sc->sc_nintr && irqno < LPI_BASE) ||
	    irqno >= LPI_BASE + sc->sc_nlpi)
		panic("agintc_intr_establish: bogus irqnumber %d: %s",
		    irqno, name);

	if (ci == NULL)
		ci = &cpu_info_primary;

	ih = malloc(sizeof *ih, M_DEVBUF, M_WAITOK);
	ih->ih_func = func;
	ih->ih_arg = arg;
	ih->ih_ipl = level & IPL_IRQMASK;
	ih->ih_flags = level & IPL_FLAGMASK;
	ih->ih_irq = irqno;
	ih->ih_name = name;
	ih->ih_ci = ci;

	psw = intr_disable();

	if (irqno < LPI_BASE) {
		if (!TAILQ_EMPTY(&sc->sc_handler[irqno].iq_list) &&
		    sc->sc_handler[irqno].iq_ci != ci) {
			intr_restore(psw);
			free(ih, M_DEVBUF, sizeof *ih);
			return NULL;
		}
		TAILQ_INSERT_TAIL(&sc->sc_handler[irqno].iq_list, ih, ih_list);
		sc->sc_handler[irqno].iq_ci = ci;
	}

	if (name != NULL)
		evcount_attach(&ih->ih_count, name, &ih->ih_irq);

#ifdef DEBUG_AGINTC
	printf("%s: irq %d level %d [%s]\n", __func__, irqno, level, name);
#endif

	if (irqno < LPI_BASE) {
		agintc_intr_config(sc, irqno, type);
		agintc_calc_irq(sc, irqno);
	} else {
		uint8_t *prop = AGINTC_DMA_KVA(sc->sc_prop);

		prop[irqno - LPI_BASE] = (((0xff - ih->ih_ipl) << 4) & 0xff) |
		    GICR_PROP_GROUP1 | GICR_PROP_ENABLE;

		/* Make globally visible. */
		cpu_dcache_wb_range((vaddr_t)&prop[irqno - LPI_BASE],
		    sizeof(*prop));
		__asm volatile("dsb sy");
	}

	intr_restore(psw);
	return (ih);
}

void
agintc_intr_disestablish(void *cookie)
{
	struct agintc_softc	*sc = agintc_sc;
	struct intrhand		*ih = cookie;
	int			 irqno = ih->ih_irq;
	u_long			 psw;
	struct agintc_mbi_range	*mr;
	int			 i;

	psw = intr_disable();

	if (irqno < LPI_BASE) {
		TAILQ_REMOVE(&sc->sc_handler[irqno].iq_list, ih, ih_list);
		agintc_calc_irq(sc, irqno);

		/* In case this is an MBI, free it */
		for (i = 0; i < sc->sc_mbi_nranges; i++) {
			mr = &sc->sc_mbi_ranges[i];
			if (irqno < mr->mr_base)
				continue;
			if (irqno >= mr->mr_base + mr->mr_span)
				break;
			if (mr->mr_mbi[irqno - mr->mr_base] != NULL)
				mr->mr_mbi[irqno - mr->mr_base] = NULL;
		}
	} else {
		uint8_t *prop = AGINTC_DMA_KVA(sc->sc_prop);

		prop[irqno - LPI_BASE] = 0;

		/* Make globally visible. */
		cpu_dcache_wb_range((vaddr_t)&prop[irqno - LPI_BASE],
		    sizeof(*prop));
		__asm volatile("dsb sy");
	}

	if (ih->ih_name != NULL)
		evcount_detach(&ih->ih_count);

	intr_restore(psw);

	free(ih, M_DEVBUF, 0);
}

void
agintc_intr_set_wakeup(void *cookie)
{
	struct intrhand *ih = cookie;

	ih->ih_flags |= IPL_WAKEUP;
}

void *
agintc_intr_establish_mbi(void *self, uint64_t *addr, uint64_t *data,
    int level, struct cpu_info *ci, int (*func)(void *), void *arg, char *name)
{
	struct agintc_softc *sc = agintc_sc;
	struct agintc_mbi_range *mr;
	void *cookie;
	int i, j, hwcpu;

	if (ci == NULL)
		ci = &cpu_info_primary;
	hwcpu = agintc_sc->sc_cpuremap[ci->ci_cpuid];

	for (i = 0; i < sc->sc_mbi_nranges; i++) {
		mr = &sc->sc_mbi_ranges[i];
		for (j = 0; j < mr->mr_span; j++) {
			if (mr->mr_mbi[j] != NULL)
				continue;

			cookie = agintc_intr_establish(mr->mr_base + j,
			    IST_EDGE_RISING, level, ci, func, arg, name);
			if (cookie == NULL)
				return NULL;

			*addr = sc->sc_mbi_addr;
			*data = mr->mr_base + j;

			mr->mr_mbi[j] = cookie;
			return cookie;
		}
	}

	return NULL;
}

void
agintc_eoi(uint32_t eoi)
{
	__asm volatile("msr "STR(ICC_EOIR1)", %x0" :: "r" (eoi));
	__isb();
}

void
agintc_d_wait_rwp(struct agintc_softc *sc)
{
	int count = 100000;
	uint32_t v;

	do {
		v = bus_space_read_4(sc->sc_iot, sc->sc_d_ioh, GICD_CTLR);
	} while (--count && (v & GICD_CTLR_RWP));

	if (count == 0)
		panic("%s: RWP timed out 0x08%x", __func__, v);
}

void
agintc_r_wait_rwp(struct agintc_softc *sc)
{
	struct cpu_info *ci = curcpu();
	int hwcpu = sc->sc_cpuremap[ci->ci_cpuid];
	int count = 100000;
	uint32_t v;

	do {
		v = bus_space_read_4(sc->sc_iot, sc->sc_r_ioh[hwcpu],
		    GICR_CTLR);
	} while (--count && (v & GICR_CTLR_RWP));

	if (count == 0)
		panic("%s: RWP timed out 0x08%x", __func__, v);
}

#ifdef MULTIPROCESSOR
int
agintc_ipi_ddb(void *v)
{
	/* XXX */
#ifdef DDB
	db_enter();
#endif
	return 1;
}

int
agintc_ipi_halt(void *v)
{
	struct agintc_softc *sc = v;
	int old = curcpu()->ci_cpl;

	intr_disable();
	agintc_eoi(sc->sc_ipi_num[ARM_IPI_HALT]);
	agintc_setipl(IPL_NONE);

	cpu_halt();

	agintc_setipl(old);
	intr_enable();
	return 1;
}

int
agintc_ipi_nop(void *v)
{
	/* Nothing to do here, just enough to wake up from WFI */
	return 1;
}

int
agintc_ipi_combined(void *v)
{
	struct agintc_softc *sc = v;

	if (sc->sc_ipi_reason[cpu_number()] == ARM_IPI_DDB) {
		sc->sc_ipi_reason[cpu_number()] = ARM_IPI_NOP;
		return agintc_ipi_ddb(v);
	} else if (sc->sc_ipi_reason[cpu_number()] == ARM_IPI_HALT) {
		sc->sc_ipi_reason[cpu_number()] = ARM_IPI_NOP;
		return agintc_ipi_halt(v);
	} else {
		return agintc_ipi_nop(v);
	}
}

void
agintc_send_ipi(struct cpu_info *ci, int id)
{
	struct agintc_softc	*sc = agintc_sc;
	uint64_t sendmask;

	if (ci == curcpu() && id == ARM_IPI_NOP)
		return;

	/* never overwrite IPI_DDB or IPI_HALT with IPI_NOP */
	if (id == ARM_IPI_DDB || id == ARM_IPI_HALT)
		sc->sc_ipi_reason[ci->ci_cpuid] = id;

	/* will only send 1 cpu */
	sendmask = (ci->ci_mpidr & MPIDR_AFF3) << 16;
	sendmask |= (ci->ci_mpidr & MPIDR_AFF2) << 16;
	sendmask |= (ci->ci_mpidr & MPIDR_AFF1) << 8;
	sendmask |= 1 << (ci->ci_mpidr & 0x0f);
	sendmask |= (sc->sc_ipi_num[id] << 24);

	__asm volatile ("msr " STR(ICC_SGI1R)", %x0" ::"r"(sendmask));
}
#endif

/*
 * GICv3 ITS controller for MSI interrupts.
 */
#define GITS_CTLR		0x0000
#define  GITS_CTLR_ENABLED	(1UL << 0)
#define GITS_TYPER		0x0008
#define  GITS_TYPER_CIL		(1ULL << 36)
#define  GITS_TYPER_CIDBITS(x)	(((x) >> 32) & 0xf)
#define  GITS_TYPER_HCC(x)	(((x) >> 24) & 0xff)
#define  GITS_TYPER_PTA		(1ULL << 19)
#define  GITS_TYPER_DEVBITS(x)	(((x) >> 13) & 0x1f)
#define  GITS_TYPER_ITE_SZ(x)	(((x) >> 4) & 0xf)
#define  GITS_TYPER_PHYS	(1ULL << 0)
#define GITS_CBASER		0x0080
#define  GITS_CBASER_VALID	(1ULL << 63)
#define  GITS_CBASER_IC_NORM_NC	(1ULL << 59)
#define  GITS_CBASER_MASK	0x1ffffffffff000ULL
#define GITS_CWRITER		0x0088
#define GITS_CREADR		0x0090
#define GITS_BASER(i)		(0x0100 + ((i) * 8))
#define  GITS_BASER_VALID	(1ULL << 63)
#define  GITS_BASER_INDIRECT	(1ULL << 62)
#define  GITS_BASER_IC_NORM_NC	(1ULL << 59)
#define  GITS_BASER_TYPE_MASK	(7ULL << 56)
#define  GITS_BASER_TYPE_DEVICE	(1ULL << 56)
#define  GITS_BASER_TYPE_COLL	(4ULL << 56)
#define  GITS_BASER_TTE_SZ(x)	(((x) >> 48) & 0x1f)
#define  GITS_BASER_PGSZ_MASK	(3ULL << 8)
#define  GITS_BASER_PGSZ_4K	(0ULL << 8)
#define  GITS_BASER_PGSZ_16K	(1ULL << 8)
#define  GITS_BASER_PGSZ_64K	(2ULL << 8)
#define  GITS_BASER_PA_MASK	0x7ffffffff000ULL
#define GITS_TRANSLATER		0x10040

#define GITS_NUM_BASER		8

struct gits_cmd {
	uint8_t cmd;
	uint32_t deviceid;
	uint32_t eventid;
	uint32_t intid;
	uint64_t dw2;
	uint64_t dw3;
};

#define GITS_CMD_VALID		(1ULL << 63)

/* ITS commands */
#define SYNC	0x05
#define MAPD	0x08
#define MAPC	0x09
#define MAPTI	0x0a
#define INV	0x0c
#define INVALL	0x0d
#define DISCARD 0x0f

#define GITS_CMDQ_SIZE		(64 * 1024)
#define GITS_CMDQ_NENTRIES	(GITS_CMDQ_SIZE / sizeof(struct gits_cmd))

struct agintc_msi_device {
	LIST_ENTRY(agintc_msi_device) md_list;

	uint32_t		md_deviceid;
	uint32_t		md_events;
	struct agintc_dmamem	*md_itt;
};

int	 agintc_msi_match(struct device *, void *, void *);
void	 agintc_msi_attach(struct device *, struct device *, void *);
void	*agintc_intr_establish_msi(void *, uint64_t *, uint64_t *,
	    int , struct cpu_info *, int (*)(void *), void *, char *);
void	 agintc_intr_disestablish_msi(void *);
void	 agintc_intr_barrier_msi(void *);

struct agintc_msi_softc {
	struct device			sc_dev;
	bus_space_tag_t			sc_iot;
	bus_space_handle_t		sc_ioh;
	bus_dma_tag_t			sc_dmat;

	bus_addr_t			sc_msi_addr;
	int				sc_msi_delta;

	struct agintc_dmamem		*sc_cmdq;
	uint16_t			sc_cmdidx;

	int				sc_devbits;
	struct agintc_dmamem		*sc_dtt;
	size_t				sc_dtt_pgsz;
	uint8_t				sc_dte_sz;
	int				sc_cidbits;
	struct agintc_dmamem		*sc_ctt;
	size_t				sc_ctt_pgsz;
	uint8_t				sc_cte_sz;
	uint8_t				sc_ite_sz;

	LIST_HEAD(, agintc_msi_device)	sc_msi_devices;

	struct interrupt_controller	sc_ic;
};

const struct cfattach	agintcmsi_ca = {
	sizeof (struct agintc_msi_softc), agintc_msi_match, agintc_msi_attach
};

struct cfdriver agintcmsi_cd = {
	NULL, "agintcmsi", DV_DULL
};

void	agintc_msi_send_cmd(struct agintc_msi_softc *, struct gits_cmd *);
void	agintc_msi_wait_cmd(struct agintc_msi_softc *);

int
agintc_msi_match(struct device *parent, void *cfdata, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "arm,gic-v3-its");
}

void
agintc_msi_attach(struct device *parent, struct device *self, void *aux)
{
	struct agintc_msi_softc *sc = (struct agintc_msi_softc *)self;
	struct fdt_attach_args *faa = aux;
	struct gits_cmd cmd;
	uint32_t pre_its[2];
	uint64_t typer;
	int i, hwcpu;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}
	sc->sc_dmat = faa->fa_dmat;

	sc->sc_msi_addr = faa->fa_reg[0].addr + GITS_TRANSLATER;
	if (OF_getpropintarray(faa->fa_node, "socionext,synquacer-pre-its",
	    pre_its, sizeof(pre_its)) == sizeof(pre_its)) {
		sc->sc_msi_addr = pre_its[0];
		sc->sc_msi_delta = 4;
	}

	typer = bus_space_read_8(sc->sc_iot, sc->sc_ioh, GITS_TYPER);
	if ((typer & GITS_TYPER_PHYS) == 0 || typer & GITS_TYPER_PTA) {
		printf(": unsupported type 0x%016llx\n", typer);
		goto unmap;
	}
	sc->sc_ite_sz = GITS_TYPER_ITE_SZ(typer) + 1;
	sc->sc_devbits = GITS_TYPER_DEVBITS(typer) + 1;
	if (typer & GITS_TYPER_CIL)
		sc->sc_cidbits = GITS_TYPER_CIDBITS(typer) + 1;
	else
		sc->sc_cidbits = 16;

	/* Set up command queue. */
	sc->sc_cmdq = agintc_dmamem_alloc(sc->sc_dmat,
	    GITS_CMDQ_SIZE, GITS_CMDQ_SIZE);
	if (sc->sc_cmdq == NULL) {
		printf(": can't alloc command queue\n");
		goto unmap;
	}
	bus_space_write_8(sc->sc_iot, sc->sc_ioh, GITS_CBASER,
	    AGINTC_DMA_DVA(sc->sc_cmdq) | GITS_CBASER_IC_NORM_NC |
	    (GITS_CMDQ_SIZE / PAGE_SIZE) - 1 | GITS_CBASER_VALID);

	/* Set up device translation table. */
	for (i = 0; i < GITS_NUM_BASER; i++) {
		uint64_t baser;
		paddr_t dtt_pa;
		size_t size;

		baser = bus_space_read_8(sc->sc_iot, sc->sc_ioh, GITS_BASER(i));
		if ((baser & GITS_BASER_TYPE_MASK) != GITS_BASER_TYPE_DEVICE)
			continue;

		/* Determine the maximum supported page size. */
		bus_space_write_8(sc->sc_iot, sc->sc_ioh, GITS_BASER(i),
		    (baser & ~GITS_BASER_PGSZ_MASK) | GITS_BASER_PGSZ_64K);
		baser = bus_space_read_8(sc->sc_iot, sc->sc_ioh, GITS_BASER(i));
		if ((baser & GITS_BASER_PGSZ_MASK) == GITS_BASER_PGSZ_64K)
			goto dfound;
		
		bus_space_write_8(sc->sc_iot, sc->sc_ioh, GITS_BASER(i),
		    (baser & ~GITS_BASER_PGSZ_MASK) | GITS_BASER_PGSZ_16K);
		baser = bus_space_read_8(sc->sc_iot, sc->sc_ioh, GITS_BASER(i));
		if ((baser & GITS_BASER_PGSZ_MASK) == GITS_BASER_PGSZ_16K)
			goto dfound;

		bus_space_write_8(sc->sc_iot, sc->sc_ioh, GITS_BASER(i),
		    (baser & ~GITS_BASER_PGSZ_MASK) | GITS_BASER_PGSZ_4K);
		baser = bus_space_read_8(sc->sc_iot, sc->sc_ioh, GITS_BASER(i));

	dfound:
		switch (baser & GITS_BASER_PGSZ_MASK) {
		case GITS_BASER_PGSZ_4K:
			sc->sc_dtt_pgsz = PAGE_SIZE;
			break;
		case GITS_BASER_PGSZ_16K:
			sc->sc_dtt_pgsz = 4 * PAGE_SIZE;
			break;
		case GITS_BASER_PGSZ_64K:
			sc->sc_dtt_pgsz = 16 * PAGE_SIZE;
			break;
		}

		/* Calculate table size. */
		sc->sc_dte_sz = GITS_BASER_TTE_SZ(baser) + 1;
		size = (1ULL << sc->sc_devbits) * sc->sc_dte_sz;
		size = roundup(size, sc->sc_dtt_pgsz);

		/* Allocate table. */
		sc->sc_dtt = agintc_dmamem_alloc(sc->sc_dmat,
		    size, sc->sc_dtt_pgsz);
		if (sc->sc_dtt == NULL) {
			printf(": can't alloc translation table\n");
			goto unmap;
		}

		/* Configure table. */
		dtt_pa = AGINTC_DMA_DVA(sc->sc_dtt);
		KASSERT((dtt_pa & GITS_BASER_PA_MASK) == dtt_pa);
		bus_space_write_8(sc->sc_iot, sc->sc_ioh, GITS_BASER(i),
		    GITS_BASER_IC_NORM_NC | baser & GITS_BASER_PGSZ_MASK | 
		    dtt_pa | (size / sc->sc_dtt_pgsz) - 1 | GITS_BASER_VALID);
	}

	/* Set up collection translation table. */
	for (i = 0; i < GITS_NUM_BASER; i++) {
		uint64_t baser;
		paddr_t ctt_pa;
		size_t size;

		baser = bus_space_read_8(sc->sc_iot, sc->sc_ioh, GITS_BASER(i));
		if ((baser & GITS_BASER_TYPE_MASK) != GITS_BASER_TYPE_COLL)
			continue;

		/* Determine the maximum supported page size. */
		bus_space_write_8(sc->sc_iot, sc->sc_ioh, GITS_BASER(i),
		    (baser & ~GITS_BASER_PGSZ_MASK) | GITS_BASER_PGSZ_64K);
		baser = bus_space_read_8(sc->sc_iot, sc->sc_ioh, GITS_BASER(i));
		if ((baser & GITS_BASER_PGSZ_MASK) == GITS_BASER_PGSZ_64K)
			goto cfound;

		bus_space_write_8(sc->sc_iot, sc->sc_ioh, GITS_BASER(i),
		    (baser & ~GITS_BASER_PGSZ_MASK) | GITS_BASER_PGSZ_16K);
		baser = bus_space_read_8(sc->sc_iot, sc->sc_ioh, GITS_BASER(i));
		if ((baser & GITS_BASER_PGSZ_MASK) == GITS_BASER_PGSZ_16K)
			goto cfound;

		bus_space_write_8(sc->sc_iot, sc->sc_ioh, GITS_BASER(i),
		    (baser & ~GITS_BASER_PGSZ_MASK) | GITS_BASER_PGSZ_4K);
		baser = bus_space_read_8(sc->sc_iot, sc->sc_ioh, GITS_BASER(i));

	cfound:
		switch (baser & GITS_BASER_PGSZ_MASK) {
		case GITS_BASER_PGSZ_4K:
			sc->sc_ctt_pgsz = PAGE_SIZE;
			break;
		case GITS_BASER_PGSZ_16K:
			sc->sc_ctt_pgsz = 4 * PAGE_SIZE;
			break;
		case GITS_BASER_PGSZ_64K:
			sc->sc_ctt_pgsz = 16 * PAGE_SIZE;
			break;
		}

		/* Calculate table size. */
		sc->sc_cte_sz = GITS_BASER_TTE_SZ(baser) + 1;
		size = (1ULL << sc->sc_cidbits) * sc->sc_cte_sz;
		size = roundup(size, sc->sc_ctt_pgsz);

		/* Allocate table. */
		sc->sc_ctt = agintc_dmamem_alloc(sc->sc_dmat,
		    size, sc->sc_ctt_pgsz);
		if (sc->sc_ctt == NULL) {
			printf(": can't alloc translation table\n");
			goto unmap;
		}

		/* Configure table. */
		ctt_pa = AGINTC_DMA_DVA(sc->sc_ctt);
		KASSERT((ctt_pa & GITS_BASER_PA_MASK) == ctt_pa);
		bus_space_write_8(sc->sc_iot, sc->sc_ioh, GITS_BASER(i),
		    GITS_BASER_IC_NORM_NC | baser & GITS_BASER_PGSZ_MASK | 
		    ctt_pa | (size / sc->sc_ctt_pgsz) - 1 | GITS_BASER_VALID);
	}

	/* Enable ITS. */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GITS_CTLR,
	    GITS_CTLR_ENABLED);

	LIST_INIT(&sc->sc_msi_devices);

	/* Create one collection per core. */
	KASSERT(ncpus <= agintc_sc->sc_num_redist);
	for (i = 0; i < ncpus; i++) {
		hwcpu = agintc_sc->sc_cpuremap[i];
		memset(&cmd, 0, sizeof(cmd));
		cmd.cmd = MAPC;
		cmd.dw2 = GITS_CMD_VALID |
		    (agintc_sc->sc_processor[hwcpu] << 16) | i;
		agintc_msi_send_cmd(sc, &cmd);
		agintc_msi_wait_cmd(sc);
	}

	printf("\n");

	sc->sc_ic.ic_node = faa->fa_node;
	sc->sc_ic.ic_cookie = sc;
	sc->sc_ic.ic_establish_msi = agintc_intr_establish_msi;
	sc->sc_ic.ic_disestablish = agintc_intr_disestablish_msi;
	sc->sc_ic.ic_barrier = agintc_intr_barrier_msi;
	sc->sc_ic.ic_gic_its_id = OF_getpropint(faa->fa_node,
	    "openbsd,gic-its-id", 0);
	arm_intr_register_fdt(&sc->sc_ic);
	return;

unmap:
	if (sc->sc_dtt)
		agintc_dmamem_free(sc->sc_dmat, sc->sc_dtt);
	if (sc->sc_cmdq)
		agintc_dmamem_free(sc->sc_dmat, sc->sc_cmdq);

	bus_space_unmap(sc->sc_iot, sc->sc_ioh, faa->fa_reg[0].size);
}

void
agintc_msi_send_cmd(struct agintc_msi_softc *sc, struct gits_cmd *cmd)
{
	struct gits_cmd *queue = AGINTC_DMA_KVA(sc->sc_cmdq);

	memcpy(&queue[sc->sc_cmdidx], cmd, sizeof(*cmd));

	/* Make globally visible. */
	cpu_dcache_wb_range((vaddr_t)&queue[sc->sc_cmdidx], sizeof(*cmd));
	__asm volatile("dsb sy");

	sc->sc_cmdidx++;
	sc->sc_cmdidx %= GITS_CMDQ_NENTRIES;
	bus_space_write_8(sc->sc_iot, sc->sc_ioh, GITS_CWRITER,
	    sc->sc_cmdidx * sizeof(*cmd));
}

void
agintc_msi_wait_cmd(struct agintc_msi_softc *sc)
{
	uint64_t creadr;
	int timo;

	for (timo = 1000; timo > 0; timo--) {
		creadr = bus_space_read_8(sc->sc_iot, sc->sc_ioh, GITS_CREADR);
		if (creadr == sc->sc_cmdidx * sizeof(struct gits_cmd))
			break;
		delay(1);
	}
	if (timo == 0)
		printf("%s: command queue timeout\n", sc->sc_dev.dv_xname);
}

struct agintc_msi_device *
agintc_msi_create_device(struct agintc_msi_softc *sc, uint32_t deviceid)
{
	struct agintc_msi_device *md;
	struct gits_cmd cmd;

	md = malloc(sizeof(*md), M_DEVBUF, M_ZERO | M_WAITOK);
	md->md_deviceid = deviceid;
	md->md_itt = agintc_dmamem_alloc(sc->sc_dmat,
	    32 * sc->sc_ite_sz, PAGE_SIZE);
	LIST_INSERT_HEAD(&sc->sc_msi_devices, md, md_list);

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = MAPD;
	cmd.deviceid = deviceid;
	cmd.eventid = 4;	/* size */
	cmd.dw2 = AGINTC_DMA_DVA(md->md_itt) | GITS_CMD_VALID;
	agintc_msi_send_cmd(sc, &cmd);
	agintc_msi_wait_cmd(sc);

	return md;
}

struct agintc_msi_device *
agintc_msi_find_device(struct agintc_msi_softc *sc, uint32_t deviceid)
{
	struct agintc_msi_device *md;

	LIST_FOREACH(md, &sc->sc_msi_devices, md_list) {
		if (md->md_deviceid == deviceid)
			return md;
	}

	return agintc_msi_create_device(sc, deviceid);
}

void
agintc_msi_discard(struct agintc_lpi_info *li)
{
	struct agintc_msi_softc *sc;
	struct cpu_info *ci;
	struct gits_cmd cmd;
	int hwcpu;

	sc = li->li_msic;
	ci = li->li_ci;
	hwcpu = agintc_sc->sc_cpuremap[ci->ci_cpuid];

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = DISCARD;
	cmd.deviceid = li->li_deviceid;
	cmd.eventid = li->li_eventid;
	agintc_msi_send_cmd(sc, &cmd);

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = SYNC;
	cmd.dw2 = agintc_sc->sc_processor[hwcpu] << 16;
	agintc_msi_send_cmd(sc, &cmd);
	agintc_msi_wait_cmd(sc);
}

void
agintc_msi_inv(struct agintc_lpi_info *li)
{
	struct agintc_msi_softc *sc;
	struct cpu_info *ci;
	struct gits_cmd cmd;
	int hwcpu;

	sc = li->li_msic;
	ci = li->li_ci;
	hwcpu = agintc_sc->sc_cpuremap[ci->ci_cpuid];

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = INV;
	cmd.deviceid = li->li_deviceid;
	cmd.eventid = li->li_eventid;
	agintc_msi_send_cmd(sc, &cmd);

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = SYNC;
	cmd.dw2 = agintc_sc->sc_processor[hwcpu] << 16;
	agintc_msi_send_cmd(sc, &cmd);
	agintc_msi_wait_cmd(sc);
}

void *
agintc_intr_establish_msi(void *self, uint64_t *addr, uint64_t *data,
    int level, struct cpu_info *ci, int (*func)(void *), void *arg, char *name)
{
	struct agintc_msi_softc *sc = (struct agintc_msi_softc *)self;
	struct agintc_msi_device *md;
	struct gits_cmd cmd;
	uint32_t deviceid = *data;
	uint32_t eventid;
	int i, hwcpu;

	if (ci == NULL)
		ci = &cpu_info_primary;
	hwcpu = agintc_sc->sc_cpuremap[ci->ci_cpuid];

	md = agintc_msi_find_device(sc, deviceid);
	if (md == NULL)
		return NULL;

	eventid = *addr;
	if (eventid > 0 && (md->md_events & (1U << eventid)))
		return NULL;
	for (; eventid < 32; eventid++) {
		if ((md->md_events & (1U << eventid)) == 0) {
			md->md_events |= (1U << eventid);
			break;
		}
	}
	if (eventid >= 32)
		return NULL;

	for (i = 0; i < agintc_sc->sc_nlpi; i++) {
		if (agintc_sc->sc_lpi[i] != NULL)
			continue;

		agintc_sc->sc_lpi[i] = malloc(sizeof(struct agintc_lpi_info),
		    M_DEVBUF, M_WAITOK | M_ZERO);
		agintc_sc->sc_lpi[i]->li_msic = sc;
		agintc_sc->sc_lpi[i]->li_ci = ci;
		agintc_sc->sc_lpi[i]->li_deviceid = deviceid;
		agintc_sc->sc_lpi[i]->li_eventid = eventid;
		agintc_sc->sc_lpi[i]->li_ih =
		    agintc_intr_establish(LPI_BASE + i,
		    IST_EDGE_RISING, level, ci, func, arg, name);
		if (agintc_sc->sc_lpi[i]->li_ih == NULL) {
			free(agintc_sc->sc_lpi[i], M_DEVBUF,
			    sizeof(struct agintc_lpi_info));
			agintc_sc->sc_lpi[i] = NULL;
			return NULL;
		}

		memset(&cmd, 0, sizeof(cmd));
		cmd.cmd = MAPTI;
		cmd.deviceid = deviceid;
		cmd.eventid = eventid;
		cmd.intid = LPI_BASE + i;
		cmd.dw2 = ci->ci_cpuid;
		agintc_msi_send_cmd(sc, &cmd);

		memset(&cmd, 0, sizeof(cmd));
		cmd.cmd = SYNC;
		cmd.dw2 = agintc_sc->sc_processor[hwcpu] << 16;
		agintc_msi_send_cmd(sc, &cmd);
		agintc_msi_wait_cmd(sc);

		*addr = sc->sc_msi_addr + deviceid * sc->sc_msi_delta;
		*data = eventid;
		return &agintc_sc->sc_lpi[i];
	}

	return NULL;
}

void
agintc_intr_disestablish_msi(void *cookie)
{
	struct agintc_lpi_info *li = *(void **)cookie;

	agintc_intr_disestablish(li->li_ih);
	agintc_msi_discard(li);
	agintc_msi_inv(li);

	free(li, M_DEVBUF, sizeof(*li));
	*(void **)cookie = NULL;
}

void
agintc_intr_barrier_msi(void *cookie)
{
	struct agintc_lpi_info *li = *(void **)cookie;

	agintc_intr_barrier(li->li_ih);
}

struct agintc_dmamem *
agintc_dmamem_alloc(bus_dma_tag_t dmat, bus_size_t size, bus_size_t align)
{
	struct agintc_dmamem *adm;
	int nsegs;

	adm = malloc(sizeof(*adm), M_DEVBUF, M_WAITOK | M_ZERO);
	adm->adm_size = size;

	if (bus_dmamap_create(dmat, size, 1, size, 0,
	    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW, &adm->adm_map) != 0)
		goto admfree;

	if (bus_dmamem_alloc(dmat, size, align, 0, &adm->adm_seg, 1,
	    &nsegs, BUS_DMA_WAITOK | BUS_DMA_ZERO) != 0)
		goto destroy;

	if (bus_dmamem_map(dmat, &adm->adm_seg, nsegs, size,
	    &adm->adm_kva, BUS_DMA_WAITOK | BUS_DMA_NOCACHE) != 0)
		goto free;

	if (bus_dmamap_load_raw(dmat, adm->adm_map, &adm->adm_seg,
	    nsegs, size, BUS_DMA_WAITOK) != 0)
		goto unmap;

	/* Make globally visible. */
	cpu_dcache_wb_range((vaddr_t)adm->adm_kva, size);
	__asm volatile("dsb sy");
	return adm;

unmap:
	bus_dmamem_unmap(dmat, adm->adm_kva, size);
free:
	bus_dmamem_free(dmat, &adm->adm_seg, 1);
destroy:
	bus_dmamap_destroy(dmat, adm->adm_map);
admfree:
	free(adm, M_DEVBUF, sizeof(*adm));

	return NULL;
}

void
agintc_dmamem_free(bus_dma_tag_t dmat, struct agintc_dmamem *adm)
{
	bus_dmamem_unmap(dmat, adm->adm_kva, adm->adm_size);
	bus_dmamem_free(dmat, &adm->adm_seg, 1);
	bus_dmamap_destroy(dmat, adm->adm_map);
	free(adm, M_DEVBUF, sizeof(*adm));
}
