/*	$OpenBSD$	*/

/*
 * Copyright (c) 2016 Visa Hankala
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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

/* Driver for OCTEON MMC host controller. */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/endian.h>
#include <sys/malloc.h>
#include <sys/rwlock.h>
#include <sys/kernel.h>

#include <dev/ofw/fdt.h>
#include <dev/ofw/openfirm.h>
#include <dev/sdmmc/sdmmcchip.h>
#include <dev/sdmmc/sdmmcvar.h>
#include <dev/sdmmc/sdmmc_ioreg.h>

#include <mips64/cache.h>

#include <machine/bus.h>
#include <machine/fdt.h>
#include <machine/octeonreg.h>
#include <machine/octeonvar.h>
#include <machine/octeon_model.h>

#include <octeon/dev/octmmcreg.h>

#define OCTMMC_BLOCK_SIZE		512
#define OCTMMC_CMD_TIMEOUT		5		/* in seconds */
#define OCTMMC_MAX_DMASEG		(1u << 18)
#define OCTMMC_MAX_FREQ			52000		/* in kHz */
#define OCTMMC_MAX_BUSES		4

#define MMC_RD_8(sc, reg) \
	bus_space_read_8((sc)->sc_iot, (sc)->sc_mmc_ioh, (reg))
#define MMC_WR_8(sc, reg, val) \
	bus_space_write_8((sc)->sc_iot, (sc)->sc_mmc_ioh, (reg), (val))
#define DMA_WR_8(sc, reg, val) \
	bus_space_write_8((sc)->sc_iot, (sc)->sc_dma_ioh, (reg), (val))

#define divround(n, d) (((n) + (d) / 2) / (d))

struct octmmc_softc;

struct octmmc_bus {
	struct octmmc_softc	*bus_hc;
	struct device		*bus_sdmmc;
	uint32_t		 bus_id;
	uint32_t		 bus_cmd_skew;
	uint32_t		 bus_dat_skew;
	uint32_t		 bus_max_freq;		/* in kHz */
	uint64_t		 bus_switch;
	uint64_t		 bus_rca;
	uint64_t		 bus_wdog;
};

struct octmmc_softc {
	struct device		 sc_dev;
	bus_space_tag_t		 sc_iot;
	bus_space_handle_t	 sc_mmc_ioh;
	bus_space_handle_t	 sc_dma_ioh;
	bus_dma_tag_t		 sc_dmat;
	bus_dmamap_t		 sc_dma_data;
	void			*sc_cmd_ih;
	void			*sc_dma_ih;

	struct octmmc_bus	 sc_buses[OCTMMC_MAX_BUSES];
	struct octmmc_bus	*sc_current_bus;

	uint64_t		 sc_current_switch;
	uint64_t		 sc_intr_status;
};

int	 octmmc_match(struct device *, void *, void *);
void	 octmmc_attach(struct device *, struct device *, void *);

int	 octmmc_host_reset(sdmmc_chipset_handle_t);
uint32_t octmmc_host_ocr(sdmmc_chipset_handle_t);
int	 octmmc_host_maxblklen(sdmmc_chipset_handle_t);
int	 octmmc_card_detect(sdmmc_chipset_handle_t);
int	 octmmc_bus_width(sdmmc_chipset_handle_t, int);
int	 octmmc_bus_power(sdmmc_chipset_handle_t, uint32_t);
int	 octmmc_bus_clock(sdmmc_chipset_handle_t, int, int);
void	 octmmc_exec_command(sdmmc_chipset_handle_t, struct sdmmc_command *);

void	 octmmc_exec_dma(struct octmmc_bus *, struct sdmmc_command *);
void	 octmmc_exec_pio(struct octmmc_bus *, struct sdmmc_command *);

void	 octmmc_acquire(struct octmmc_bus *);
void	 octmmc_release(struct octmmc_bus *);

void	 octmmc_get_response(struct octmmc_softc *, struct sdmmc_command *);
int	 octmmc_init_bus(struct octmmc_bus *);
int	 octmmc_intr(void *);
int	 octmmc_wait_intr(struct octmmc_softc *, uint64_t, int);

const struct cfattach octmmc_ca = {
	sizeof(struct octmmc_softc), octmmc_match, octmmc_attach
};

struct cfdriver octmmc_cd = {
	NULL, "octmmc", DV_DULL
};

struct sdmmc_chip_functions octmmc_funcs = {
	.host_reset	= octmmc_host_reset,
	.host_ocr	= octmmc_host_ocr,
	.host_maxblklen	= octmmc_host_maxblklen,
	.card_detect	= octmmc_card_detect,
	.bus_power	= octmmc_bus_power,
	.bus_clock	= octmmc_bus_clock,
	.bus_width	= octmmc_bus_width,
	.exec_command	= octmmc_exec_command,
};

struct rwlock octmmc_lock = RWLOCK_INITIALIZER("octmmclk");

int
octmmc_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *fa = aux;

	return OF_is_compatible(fa->fa_node, "cavium,octeon-6130-mmc");
}

void
octmmc_attach(struct device *parent, struct device *self, void *aux)
{
	struct sdmmcbus_attach_args saa;
	struct fdt_attach_args *fa = aux;
	struct octmmc_softc *sc = (struct octmmc_softc *)self;
	struct octmmc_bus *bus;
	uint64_t reg;
	uint32_t bus_id, bus_width;
	int node;

	if (fa->fa_nreg < 2) {
		printf(": expected 2 IO spaces, got %d\n", fa->fa_nreg);
		return;
	}

	sc->sc_iot = fa->fa_iot;
	sc->sc_dmat = fa->fa_dmat;
	if (bus_space_map(sc->sc_iot, fa->fa_reg[0].addr, fa->fa_reg[0].size, 0,
	    &sc->sc_mmc_ioh)) {
		printf(": could not map MMC registers\n");
		goto error;
	}
	if (bus_space_map(sc->sc_iot, fa->fa_reg[1].addr, fa->fa_reg[1].size, 0,
	    &sc->sc_dma_ioh)) {
		printf(": could not map DMA registers\n");
		goto error;
	}
	if (bus_dmamap_create(sc->sc_dmat, OCTMMC_MAX_DMASEG, 1,
	    OCTMMC_MAX_DMASEG, 0, BUS_DMA_NOWAIT, &sc->sc_dma_data)) {
		printf(": could not create data dmamap\n");
		goto error;
	}

	/* Disable all buses. */
	reg = MMC_RD_8(sc, MIO_EMM_CFG);
	reg &= ~((1u << OCTMMC_MAX_BUSES) - 1);
	MMC_WR_8(sc, MIO_EMM_CFG, reg);

	/* Clear any pending interrupts. */
	reg = MMC_RD_8(sc, MIO_EMM_INT);
	MMC_WR_8(sc, MIO_EMM_INT, reg);

	sc->sc_cmd_ih = octeon_intr_establish_fdt_idx(fa->fa_node, 0, IPL_SDMMC,
	    octmmc_intr, sc, DEVNAME(sc));
	sc->sc_dma_ih = octeon_intr_establish_fdt_idx(fa->fa_node, 1, IPL_SDMMC,
	    octmmc_intr, sc, DEVNAME(sc));
	if (sc->sc_cmd_ih == NULL || sc->sc_dma_ih == NULL)
		goto error;

	printf("\n");

	sc->sc_current_bus = NULL;
	sc->sc_current_switch = ~0ull;

	for (node = OF_child(fa->fa_node); node != 0; node = OF_peer(node)) {
		if (!OF_is_compatible(node, "cavium,octeon-6130-mmc-slot"))
			continue;
		bus_id = OF_getpropint(node, "reg", (uint32_t)-1);
		if (bus_id >= OCTMMC_MAX_BUSES)
			continue;

		bus = &sc->sc_buses[bus_id];
		bus->bus_hc = sc;
		bus->bus_id = bus_id;
		bus->bus_cmd_skew = OF_getpropint(node,
		    "cavium,cmd-clk-skew", 0);
		bus->bus_dat_skew = OF_getpropint(node,
		    "cavium,dat-clk-skew", 0);

		bus->bus_max_freq = OF_getpropint(node,
		    "spi-max-frequency", 0) / 1000;
		if (bus->bus_max_freq == 0 ||
		    bus->bus_max_freq > OCTMMC_MAX_FREQ)
			bus->bus_max_freq = OCTMMC_MAX_FREQ;

		bus_width = OF_getpropint(node, "bus-width", 0);
		if (bus_width == 0)
			bus_width = OF_getpropint(node,
			    "cavium,bus-max-width", 8);

		if (octmmc_init_bus(bus)) {
			printf("%s: could not init bus %d\n", DEVNAME(sc),
			    bus_id);
			continue;
		}

		memset(&saa, 0, sizeof(saa));
		saa.saa_busname = "sdmmc";
		saa.sct = &octmmc_funcs;
		saa.sch = bus;
		saa.caps = SMC_CAPS_MMC_HIGHSPEED;
		if (bus_width >= 8)
			saa.caps |= SMC_CAPS_8BIT_MODE;
		if (bus_width >= 4)
			saa.caps |= SMC_CAPS_4BIT_MODE;

		bus->bus_sdmmc = config_found(&sc->sc_dev, &saa, NULL);
		if (bus->bus_sdmmc == NULL)
			printf("%s: bus %d: could not attach sdmmc\n",
			    DEVNAME(sc), bus_id);
	}
	return;

error:
	if (sc->sc_dma_ih != NULL)
		octeon_intr_disestablish(sc->sc_dma_ih);
	if (sc->sc_cmd_ih != NULL)
		octeon_intr_disestablish(sc->sc_cmd_ih);
	if (sc->sc_dma_data != NULL)
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_dma_data);
	if (sc->sc_dma_ioh != 0)
		bus_space_unmap(sc->sc_iot, sc->sc_dma_ioh, fa->fa_reg[1].size);
	if (sc->sc_mmc_ioh != 0)
		bus_space_unmap(sc->sc_iot, sc->sc_mmc_ioh, fa->fa_reg[0].size);
}

void
octmmc_acquire(struct octmmc_bus *bus)
{
	struct octmmc_softc *sc = bus->bus_hc;
	uint64_t period, sample;

	splassert(IPL_SDMMC);

	rw_enter_write(&octmmc_lock);

	/* Acquire the bootbus. */
	octeon_xkphys_write_8(MIO_BOOT_CFG, 0);

	if (sc->sc_current_bus == bus &&
	    sc->sc_current_switch == bus->bus_switch)
		return;

	/* Save relative card address. */
	if (sc->sc_current_bus != NULL)
		sc->sc_current_bus->bus_rca = MMC_RD_8(sc, MIO_EMM_RCA);

	sc->sc_current_bus = NULL;
	sc->sc_current_switch = ~0ull;

	/* Set bus parameters. */
	MMC_WR_8(sc, MIO_EMM_SWITCH, bus->bus_switch &
	    ~MIO_EMM_SWITCH_BUS_ID);
	MMC_WR_8(sc, MIO_EMM_SWITCH, bus->bus_switch);

	/* Set relative card address. */
	MMC_WR_8(sc, MIO_EMM_RCA, bus->bus_rca);

	/* Set command timeout. */
	MMC_WR_8(sc, MIO_EMM_WDOG, bus->bus_wdog);

	/* Set sampling skew parameters. */
	period = 1000000000000ull / octeon_ioclock_speed();
	sample = (divround(bus->bus_cmd_skew, period) <<
	    MIO_EMM_SAMPLE_CMD_CNT_SHIFT) &
	    MIO_EMM_SAMPLE_CMD_CNT;
	sample |= (divround(bus->bus_dat_skew, period) <<
	    MIO_EMM_SAMPLE_DAT_CNT_SHIFT) &
	    MIO_EMM_SAMPLE_DAT_CNT;
	MMC_WR_8(bus->bus_hc, MIO_EMM_SAMPLE, sample);

	sc->sc_current_bus = bus;
	sc->sc_current_switch = bus->bus_switch;
	return;
}

void
octmmc_release(struct octmmc_bus *bus)
{
	rw_exit_write(&octmmc_lock);
}

int
octmmc_init_bus(struct octmmc_bus *bus)
{
	struct octmmc_softc *sc = bus->bus_hc;
	uint64_t init_freq = SDMMC_SDCLK_400KHZ;
	uint64_t period;
	uint64_t reg;
	int s;

	period = divround(octeon_ioclock_speed(), init_freq * 1000 * 2);
	if (period > MIO_EMM_SWITCH_CLK_MAX)
		period = MIO_EMM_SWITCH_CLK_MAX;

	/* Set initial parameters. */
	bus->bus_switch = (uint64_t)bus->bus_id << MIO_EMM_SWITCH_BUS_ID_SHIFT;
	bus->bus_switch |= 10ull << MIO_EMM_SWITCH_POWER_CLASS_SHIFT;
	bus->bus_switch |= period << MIO_EMM_SWITCH_CLK_HI_SHIFT;
	bus->bus_switch |= period << MIO_EMM_SWITCH_CLK_LO_SHIFT;
	bus->bus_rca = 1;

	/* Make hardware timeout happen before timeout in software. */
	bus->bus_wdog = init_freq * 900 * OCTMMC_CMD_TIMEOUT;
	if (bus->bus_wdog > MIO_EMM_WDOG_CLK_CNT)
		bus->bus_wdog = MIO_EMM_WDOG_CLK_CNT;

	s = splsdmmc();

	/* Enable the bus. */
	reg = MMC_RD_8(sc, MIO_EMM_CFG);
	reg |= 1u << bus->bus_id;
	MMC_WR_8(sc, MIO_EMM_CFG, reg);

	octmmc_acquire(bus);

	/* Enable interrupts. */
	MMC_WR_8(sc, MIO_EMM_INT_EN,
	    MIO_EMM_INT_CMD_ERR | MIO_EMM_INT_CMD_DONE |
	    MIO_EMM_INT_DMA_ERR | MIO_EMM_INT_DMA_DONE);

	/* Magic from Linux. */
	MMC_WR_8(sc, MIO_EMM_STS_MASK, 0xe4390080ul);

	octmmc_release(bus);

	splx(s);

	return 0;
}

int
octmmc_intr(void *arg)
{
	struct octmmc_softc *sc = arg;
	uint64_t isr;

	/* Get and acknowledge pending interrupts. */
	isr = MMC_RD_8(sc, MIO_EMM_INT);
	if (isr == 0)
		return 0;
	MMC_WR_8(sc, MIO_EMM_INT, isr);

	if (ISSET(isr, MIO_EMM_INT_CMD_DONE) ||
	    ISSET(isr, MIO_EMM_INT_CMD_ERR) ||
	    ISSET(isr, MIO_EMM_INT_DMA_DONE) ||
	    ISSET(isr, MIO_EMM_INT_DMA_ERR)) {
		sc->sc_intr_status |= isr;
		wakeup(&sc->sc_intr_status);
	}

	return 1;
}

int
octmmc_host_reset(sdmmc_chipset_handle_t sch)
{
	struct octmmc_bus *bus = sch;
	int s;

	/* Force reswitch. */
	bus->bus_hc->sc_current_switch = ~0ull;

	s = splsdmmc();
	octmmc_acquire(bus);
	octmmc_release(bus);
	splx(s);

	return 0;
}

uint32_t
octmmc_host_ocr(sdmmc_chipset_handle_t sch)
{
	/* The hardware does only 3.3V. */
	return MMC_OCR_3_2V_3_3V | MMC_OCR_3_3V_3_4V;
}

int
octmmc_host_maxblklen(sdmmc_chipset_handle_t sch)
{
	return OCTMMC_BLOCK_SIZE;
}

int
octmmc_card_detect(sdmmc_chipset_handle_t sch)
{
	return 1;
}

int
octmmc_bus_power(sdmmc_chipset_handle_t sch, uint32_t ocr)
{
	if (ocr == 0)
		octmmc_host_reset(sch);

	return 0;
}

int
octmmc_bus_clock(sdmmc_chipset_handle_t sch, int freq, int timing)
{
	struct octmmc_bus *bus = sch;
	uint64_t ioclock = octeon_ioclock_speed();
	uint64_t period;

	if (freq == 0)
		return 0;
	if (freq > bus->bus_max_freq)
		freq = bus->bus_max_freq;
	period = divround(ioclock, freq * 1000 * 2);
	if (period > MIO_EMM_SWITCH_CLK_MAX)
		period = MIO_EMM_SWITCH_CLK_MAX;

	bus->bus_switch &= ~MIO_EMM_SWITCH_CLK_HI;
	bus->bus_switch &= ~MIO_EMM_SWITCH_CLK_LO;
	bus->bus_switch |= period << MIO_EMM_SWITCH_CLK_HI_SHIFT;
	bus->bus_switch |= period << MIO_EMM_SWITCH_CLK_LO_SHIFT;

	/* Make hardware timeout happen before timeout in software. */
	bus->bus_wdog = freq * 900 * OCTMMC_CMD_TIMEOUT;

	if (timing)
		bus->bus_switch |= MIO_EMM_SWITCH_HS_TIMING;
	else
		bus->bus_switch &= ~MIO_EMM_SWITCH_HS_TIMING;

	return 0;
}

int
octmmc_bus_width(sdmmc_chipset_handle_t sch, int width)
{
	struct octmmc_bus *bus = sch;
	uint64_t bus_width;

	switch (width) {
	case 1:
		bus_width = 0;
		break;
	case 4:
		bus_width = 1;
		break;
	case 8:
		bus_width = 2;
		break;
	default:
		return ENOTSUP;
	}
	bus->bus_switch &= ~MIO_EMM_SWITCH_BUS_WIDTH;
	bus->bus_switch |= bus_width << MIO_EMM_SWITCH_BUS_WIDTH_SHIFT;

	return 0;
}

void
octmmc_exec_command(sdmmc_chipset_handle_t sch, struct sdmmc_command *cmd)
{
	struct octmmc_bus *bus = sch;

	/*
	 * Refuse SDIO setup. The controller cannot run SDIO commands
	 * without tricks.
	 */
	if (cmd->c_opcode == SD_IO_SEND_OP_COND ||
	    cmd->c_opcode == SD_IO_RW_DIRECT) {
		cmd->c_error = ENOTSUP;
		return;
	}

	/*
	 * The DMA mode can only do data block transfers. Other commands have
	 * to use the PIO mode. Single-block transfers can use PIO because
	 * it has low setup overhead.
	 */
	if (cmd->c_opcode == MMC_READ_BLOCK_MULTIPLE ||
	    cmd->c_opcode == MMC_WRITE_BLOCK_MULTIPLE)
		octmmc_exec_dma(bus, cmd);
	else
		octmmc_exec_pio(bus, cmd);
}

void
octmmc_exec_dma(struct octmmc_bus *bus, struct sdmmc_command *cmd)
{
	struct octmmc_softc *sc = bus->bus_hc;
	uint64_t dmacfg, dmacmd, status;
	paddr_t locked_block = 0;
	caddr_t kva;
	bus_dma_segment_t segs;
	int rsegs, s;

	if (cmd->c_datalen > OCTMMC_MAX_DMASEG) {
		cmd->c_error = ENOMEM;
		return;
	}

	/* Acquire a physically contiguous buffer for DMA. */
	cmd->c_error = bus_dmamem_alloc(sc->sc_dmat, cmd->c_datalen, 0, 0,
	    &segs, 1, &rsegs, BUS_DMA_WAITOK);
	if (cmd->c_error != 0)
		return;
	cmd->c_error = bus_dmamem_map(sc->sc_dmat, &segs, rsegs, cmd->c_datalen,
	    &kva, BUS_DMA_WAITOK | BUS_DMA_COHERENT);
	if (cmd->c_error != 0)
		goto free_dma;
	cmd->c_error = bus_dmamap_load(sc->sc_dmat, sc->sc_dma_data, kva,
	    cmd->c_datalen, NULL, BUS_DMA_WAITOK);
	if (cmd->c_error != 0)
		goto unmap_dma;

	s = splsdmmc();
	octmmc_acquire(bus);

	/* Sync the buffer before DMA. */
	if (ISSET(cmd->c_flags, SCF_CMD_READ)) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dma_data, 0, cmd->c_datalen,
		    BUS_DMASYNC_PREREAD);
	} else {
		/* Copy data to the bounce buffer. */
		memcpy(kva, cmd->c_data, cmd->c_datalen);
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dma_data, 0, cmd->c_datalen,
		    BUS_DMASYNC_PREWRITE);
	}

	/*
	 * According to Linux, the DMA hardware has a silicon bug that can
	 * corrupt data. As a workaround, Linux locks the second last block
	 * of a transfer into the L2 cache if the opcode is multi-block write
	 * and there are more than two data blocks to write. In Linux, it is
	 * not described under what circumstances the corruption can happen.
	 * Lacking better information, use the same workaround here.
	 */
	if (cmd->c_opcode == MMC_WRITE_BLOCK_MULTIPLE &&
	    cmd->c_datalen > OCTMMC_BLOCK_SIZE * 2) {
		locked_block = sc->sc_dma_data->dm_segs[0].ds_addr +
		    sc->sc_dma_data->dm_segs[0].ds_len - OCTMMC_BLOCK_SIZE * 2;
		Octeon_lock_secondary_cache(curcpu(), locked_block,
		    OCTMMC_BLOCK_SIZE);
	}

	/* Set up the DMA engine. */
	dmacfg = MIO_NDF_DMA_CFG_EN;
	if (!ISSET(cmd->c_flags, SCF_CMD_READ))
		dmacfg |= MIO_NDF_DMA_CFG_RW;
	dmacfg |= (sc->sc_dma_data->dm_segs[0].ds_len / sizeof(uint64_t) - 1)
	    << MIO_NDF_DMA_CFG_SIZE_SHIFT;
	dmacfg |= sc->sc_dma_data->dm_segs[0].ds_addr;
	DMA_WR_8(sc, MIO_NDF_DMA_CFG, dmacfg);

	/* Prepare and issue the command. */
	dmacmd = MIO_EMM_DMA_DMA_VAL | MIO_EMM_DMA_MULTI | MIO_EMM_DMA_SECTOR;
	dmacmd |= (uint64_t)bus->bus_id << MIO_EMM_DMA_BUS_ID_SHIFT;
	dmacmd |= (uint64_t)(cmd->c_datalen / cmd->c_blklen) <<
	    MIO_EMM_DMA_BLOCK_CNT_SHIFT;
	dmacmd |= cmd->c_arg;
	if (!ISSET(cmd->c_flags, SCF_CMD_READ))
		dmacmd |= MIO_EMM_DMA_RW;
	MMC_WR_8(sc, MIO_EMM_DMA, dmacmd);

wait_intr:
	cmd->c_error = octmmc_wait_intr(sc, MIO_EMM_INT_CMD_ERR |
	    MIO_EMM_INT_DMA_DONE | MIO_EMM_INT_DMA_ERR,
	    OCTMMC_CMD_TIMEOUT * hz);

	status = MMC_RD_8(sc, MIO_EMM_RSP_STS);

	/* Check DMA engine error. */
	if (ISSET(sc->sc_intr_status, MIO_EMM_INT_DMA_ERR)) {
		printf("%s: dma error\n", bus->bus_sdmmc->dv_xname);

		if (ISSET(status, MIO_EMM_RSP_STS_DMA_PEND)) {
			/* Try to stop the DMA engine. */
			dmacmd = MMC_RD_8(sc, MIO_EMM_DMA);
			dmacmd |= MIO_EMM_DMA_DMA_VAL | MIO_EMM_DMA_DAT_NULL;
			dmacmd &= ~MIO_EMM_DMA_BUS_ID;
			dmacmd |= (uint64_t)bus->bus_id <<
			    MIO_EMM_DMA_BUS_ID_SHIFT;
			MMC_WR_8(sc, MIO_EMM_DMA, dmacmd);
			goto wait_intr;
		}
		cmd->c_error = EIO;
	}
	if (cmd->c_error != 0)
		goto unload_dma;

	/* Check command status. */
	if (((status & MIO_EMM_RSP_STS_CMD_IDX) >>
	    MIO_EMM_RSP_STS_CMD_IDX_SHIFT) != cmd->c_opcode ||
	    ISSET(status, MIO_EMM_RSP_STS_BLK_CRC_ERR) ||
	    ISSET(status, MIO_EMM_RSP_STS_DBUF_ERR) ||
	    ISSET(status, MIO_EMM_RSP_STS_RSP_BAD_STS) ||
	    ISSET(status, MIO_EMM_RSP_STS_RSP_CRC_ERR)) {
		cmd->c_error = EIO;
		goto unload_dma;
	}
	if (ISSET(status, MIO_EMM_RSP_STS_BLK_TIMEOUT) ||
	    ISSET(status, MIO_EMM_RSP_STS_RSP_TIMEOUT)) {
		cmd->c_error = ETIMEDOUT;
		goto unload_dma;
	}

	if (ISSET(cmd->c_flags, SCF_RSP_PRESENT))
		octmmc_get_response(sc, cmd);

	/* Sync the buffer after DMA. */
	if (ISSET(cmd->c_flags, SCF_CMD_READ)) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dma_data, 0, cmd->c_datalen,
		    BUS_DMASYNC_POSTREAD);
		/* Copy data from the bounce buffer. */
		memcpy(cmd->c_data, kva, cmd->c_datalen);
	} else {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_dma_data, 0, cmd->c_datalen,
		    BUS_DMASYNC_POSTWRITE);
	}

unload_dma:
	if (locked_block != 0)
		Octeon_unlock_secondary_cache(curcpu(), locked_block,
		    OCTMMC_BLOCK_SIZE);

	octmmc_release(bus);
	splx(s);

	bus_dmamap_unload(sc->sc_dmat, sc->sc_dma_data);
unmap_dma:
	bus_dmamem_unmap(sc->sc_dmat, kva, cmd->c_datalen);
free_dma:
	bus_dmamem_free(sc->sc_dmat, &segs, rsegs);
}

void
octmmc_exec_pio(struct octmmc_bus *bus, struct sdmmc_command *cmd)
{
	struct octmmc_softc *sc = bus->bus_hc;
	unsigned char *ptr;
	uint64_t index, piocmd, status, value;
	unsigned int i;
	int s;

	if (cmd->c_datalen > OCTMMC_BLOCK_SIZE ||
	    cmd->c_datalen % sizeof(uint64_t) != 0) {
		cmd->c_error = EINVAL;
		return;
	}

	s = splsdmmc();
	octmmc_acquire(bus);

	/* If this is a write, copy data to the controller's buffer. */
	if (cmd->c_data != NULL && !ISSET(cmd->c_flags, SCF_CMD_READ)) {
		/* Reset index and set autoincrement. */
		MMC_WR_8(sc, MIO_EMM_BUF_IDX, MIO_EMM_BUF_IDX_INC);

		ptr = cmd->c_data;
		for (i = 0; i < cmd->c_datalen / sizeof(value); i++) {
			memcpy(&value, ptr, sizeof(value));
			MMC_WR_8(sc, MIO_EMM_BUF_DAT, value);
			ptr += sizeof(value);
		}
	}

	/* Issue the command. */
	piocmd = MIO_EMM_CMD_CMD_VAL;
	piocmd |= (uint64_t)bus->bus_id << MIO_EMM_CMD_BUS_ID_SHIFT;
	piocmd |= (uint64_t)cmd->c_opcode << MIO_EMM_CMD_CMD_IDX_SHIFT;
	piocmd |= cmd->c_arg;
	MMC_WR_8(sc, MIO_EMM_CMD, piocmd);

	cmd->c_error = octmmc_wait_intr(sc, MIO_EMM_INT_CMD_DONE,
	    OCTMMC_CMD_TIMEOUT * hz);
	if (cmd->c_error != 0)
		goto pio_out;
	if (ISSET(sc->sc_intr_status, MIO_EMM_INT_CMD_ERR)) {
		cmd->c_error = EIO;
		goto pio_out;
	}

	/* Check command status. */
	status = MMC_RD_8(sc, MIO_EMM_RSP_STS);
	if (((status & MIO_EMM_RSP_STS_CMD_IDX) >>
	    MIO_EMM_RSP_STS_CMD_IDX_SHIFT) != cmd->c_opcode ||
	    ISSET(status, MIO_EMM_RSP_STS_BLK_CRC_ERR) ||
	    ISSET(status, MIO_EMM_RSP_STS_DBUF_ERR) ||
	    ISSET(status, MIO_EMM_RSP_STS_RSP_BAD_STS) ||
	    ISSET(status, MIO_EMM_RSP_STS_RSP_CRC_ERR)) {
		cmd->c_error = EIO;
		goto pio_out;
	}
	if (ISSET(status, MIO_EMM_RSP_STS_BLK_TIMEOUT) ||
	    ISSET(status, MIO_EMM_RSP_STS_RSP_TIMEOUT)) {
		cmd->c_error = ETIMEDOUT;
		goto pio_out;
	}

	if (ISSET(cmd->c_flags, SCF_RSP_PRESENT))
		octmmc_get_response(sc, cmd);

	/* If this is a read, copy data from the controller's buffer. */
	if (cmd->c_data != NULL && ISSET(cmd->c_flags, SCF_CMD_READ)) {
		/* Reset index, set autoincrement and select the buffer. */
		index = MIO_EMM_BUF_IDX_INC;
		index |= (status >> MIO_EMM_RSP_STS_DBUF_SHIFT) <<
		    MIO_EMM_BUF_IDX_BUF_NUM_SHIFT;
		MMC_WR_8(sc, MIO_EMM_BUF_IDX, MIO_EMM_BUF_IDX_INC);

		ptr = cmd->c_data;
		for (i = 0; i < cmd->c_datalen / sizeof(value); i++) {
			value = MMC_RD_8(sc, MIO_EMM_BUF_DAT);
			memcpy(ptr, &value, sizeof(value));
			ptr += sizeof(value);
		}
	}

pio_out:
	octmmc_release(bus);
	splx(s);
}

void
octmmc_get_response(struct octmmc_softc *sc, struct sdmmc_command *cmd)
{
	uint64_t hi, lo;

	if (ISSET(cmd->c_flags, SCF_RSP_136)) {
		hi = MMC_RD_8(sc, MIO_EMM_RSP_HI);
		lo = MMC_RD_8(sc, MIO_EMM_RSP_LO);

		/* Discard the checksum. */
		lo = (lo >> 8) | (hi << 56);
		hi >>= 8;

		/* The stack expects long response in little endian order. */
		cmd->c_resp[0] = htole32(lo & 0xffffffffu);
		cmd->c_resp[1] = htole32(lo >> 32);
		cmd->c_resp[2] = htole32(hi & 0xffffffffu);
		cmd->c_resp[3] = htole32(hi >> 32);
	} else {
		cmd->c_resp[0] = MMC_RD_8(sc, MIO_EMM_RSP_LO) >> 8;
	}
}

int
octmmc_wait_intr(struct octmmc_softc *sc, uint64_t mask, int timeout)
{
	splassert(IPL_SDMMC);

	mask |= MIO_EMM_INT_CMD_ERR | MIO_EMM_INT_DMA_ERR;

	sc->sc_intr_status = 0;
	while ((sc->sc_intr_status & mask) == 0) {
		if (tsleep(&sc->sc_intr_status, PWAIT, "hcintr", timeout)
		    == EWOULDBLOCK)
			return ETIMEDOUT;
	}
	return 0;
}
