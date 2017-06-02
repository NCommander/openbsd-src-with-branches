/*	$OpenBSD: xhci_fdt.c,v 1.2 2017/03/12 11:46:22 kettenis Exp $	*/
/*
 * Copyright (c) 2017 Mark kettenis <kettenis@openbsd.org>
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

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/fdt.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/xhcireg.h>
#include <dev/usb/xhcivar.h>

struct xhci_fdt_softc {
	struct xhci_softc	sc;
	int			sc_node;
	bus_space_handle_t	ph_ioh;
	void			*sc_ih;
};

int	xhci_fdt_match(struct device *, void *, void *);
void	xhci_fdt_attach(struct device *, struct device *, void *);

struct cfattach xhci_fdt_ca = {
	sizeof(struct xhci_fdt_softc), xhci_fdt_match, xhci_fdt_attach
};

void	xhci_init_phys(struct xhci_fdt_softc *);

int
xhci_fdt_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "generic-xhci") ||
	    OF_is_compatible(faa->fa_node, "snps,dwc3");
}

void
xhci_fdt_attach(struct device *parent, struct device *self, void *aux)
{
	struct xhci_fdt_softc *sc = (struct xhci_fdt_softc *)self;
	struct fdt_attach_args *faa = aux;
	int error;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_node = faa->fa_node;
	sc->sc.iot = faa->fa_iot;
	sc->sc.sc_size = faa->fa_reg[0].size;
	sc->sc.sc_bus.dmatag = faa->fa_dmat;

	if (bus_space_map(sc->sc.iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc.ioh)) {
		printf(": can't map registers\n");
		return;
	}

	sc->sc_ih = arm_intr_establish_fdt(faa->fa_node, IPL_USB,
	    xhci_intr, sc, sc->sc.sc_bus.bdev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": can't establish interrupt\n");
		goto unmap;
	}

	printf("\n");

	xhci_init_phys(sc);

	if ((error = xhci_init(&sc->sc)) != 0) {
		printf("%s: init failed, error=%d\n",
		    sc->sc.sc_bus.bdev.dv_xname, error);
		goto disestablish_ret;
	}

	/* Attach usb device. */
	config_found(self, &sc->sc.sc_bus, usbctlprint);

	/* Now that the stack is ready, config' the HC and enable interrupts. */
	xhci_config(&sc->sc);

	return;

disestablish_ret:
	arm_intr_disestablish_fdt(sc->sc_ih);
unmap:
	bus_space_unmap(sc->sc.iot, sc->sc.ioh, sc->sc.sc_size);
}

struct xhci_phy {
	const char *compat;
	void (*init)(struct xhci_fdt_softc *, uint32_t *);
};

void exynos5_usbdrd_init(struct xhci_fdt_softc *, uint32_t *);

struct xhci_phy xhci_phys[] = {
	{ "samsung,exynos5250-usbdrd-phy", exynos5_usbdrd_init },
	{ "samsung,exynos5420-usbdrd-phy", exynos5_usbdrd_init }
};

uint32_t *
xhci_next_phy(uint32_t *cells)
{
	uint32_t phandle = cells[0];
	int node, ncells;

	node = OF_getnodebyphandle(phandle);
	if (node == 0)
		return NULL;

	ncells = OF_getpropint(node, "#phy-cells", 0);
	return cells + ncells + 1;
}

void
xhci_init_phy(struct xhci_fdt_softc *sc, uint32_t *cells)
{
	int node;
	int i;

	node = OF_getnodebyphandle(cells[0]);
	if (node == 0)
		return;

	for (i = 0; i < nitems(xhci_phys); i++) {
		if (OF_is_compatible(node, xhci_phys[i].compat)) {
			xhci_phys[i].init(sc, cells);
			return;
		}
	}
}

void
xhci_init_phys(struct xhci_fdt_softc *sc)
{
	uint32_t *phys;
	uint32_t *phy;
	int len, idx;

	/* XXX Only initialize the USB 3 PHY for now. */
	idx = OF_getindex(sc->sc_node, "usb3-phy", "phy-names");
	if (idx < 0)
		return;

	len = OF_getproplen(sc->sc_node, "phys");
	if (len <= 0)
		return;

	phys = malloc(len, M_TEMP, M_WAITOK);
	OF_getpropintarray(sc->sc_node, "phys", phys, len);

	phy = phys;
	while (phy && phy < phys + (len / sizeof(uint32_t))) {
		if (idx == 0) {
			xhci_init_phy(sc, phy);
			free(phys, M_TEMP, len);
			return;
		}

		phy = xhci_next_phy(phy);
		idx--;
	}
	free(phys, M_TEMP, len);
}

/*
 * Samsung Exynos 5 PHYs.
 */

/* Registers */
#define EXYNOS5_PHYUTMI			0x0008
#define  EXYNOS5_PHYUTMI_OTGDISABLE	(1 << 6)
#define EXYNOS5_PHYCLKRST		0x0010
#define  EXYNOS5_PHYCLKRST_SSC_EN	(1 << 20)
#define  EXYNOS5_PHYCLKRST_REF_SSP_EN	(1 << 19)
#define  EXYNOS5_PHYCLKRST_PORTRESET	(1 << 1)
#define  EXYNOS5_PHYCLKRST_COMMONONN	(1 << 0)
#define EXYNOS5_PHYTEST			0x0028
#define  EXYNOS5_PHYTEST_POWERDOWN_SSP	(1 << 3)
#define  EXYNOS5_PHYTEST_POWERDOWN_HSP	(1 << 2)

/* PMU registers */
#define EXYNOS5_USBDRD0_POWER		0x0704
#define EXYNOS5420_USBDRD1_POWER	0x0708
#define  EXYNOS5_USBDRD_POWER_EN	(1 << 0)

void
exynos5_usbdrd_init(struct xhci_fdt_softc *sc, uint32_t *cells)
{
	uint32_t phy_reg[2];
	struct regmap *pmurm;
	uint32_t pmureg;
	uint32_t val;
	bus_size_t offset;
	int node;

	node = OF_getnodebyphandle(cells[0]);
	KASSERT(node != 0);

	if (OF_getpropintarray(node, "reg", phy_reg,
	    sizeof(phy_reg)) != sizeof(phy_reg))
		return;

	if (bus_space_map(sc->sc.iot, phy_reg[0],
	    phy_reg[1], 0, &sc->ph_ioh)) {
		printf("%s: can't map PHY registers\n",
		    sc->sc.sc_bus.bdev.dv_xname);
		return;
	}

	/* Power up the PHY block. */
	pmureg = OF_getpropint(node, "samsung,pmu-syscon", 0);
	pmurm = regmap_byphandle(pmureg);
	if (pmurm) {
		node = OF_getnodebyphandle(pmureg);
		if (sc->sc.sc_bus.bdev.dv_unit == 0)
			offset = EXYNOS5_USBDRD0_POWER;
		else
			offset = EXYNOS5420_USBDRD1_POWER;

		val = regmap_read_4(pmurm, offset);
		val |= EXYNOS5_USBDRD_POWER_EN;
		regmap_write_4(pmurm, offset, val);
	}

	/* Initialize the PHY.  Assumes U-Boot has done initial setup. */
	val = bus_space_read_4(sc->sc.iot, sc->ph_ioh, EXYNOS5_PHYTEST);
	CLR(val, EXYNOS5_PHYTEST_POWERDOWN_SSP);
	CLR(val, EXYNOS5_PHYTEST_POWERDOWN_HSP);
	bus_space_write_4(sc->sc.iot, sc->ph_ioh, EXYNOS5_PHYTEST, val);

	bus_space_write_4(sc->sc.iot, sc->ph_ioh, EXYNOS5_PHYUTMI,
	    EXYNOS5_PHYUTMI_OTGDISABLE);

	val = bus_space_read_4(sc->sc.iot, sc->ph_ioh, EXYNOS5_PHYCLKRST);
	SET(val, EXYNOS5_PHYCLKRST_SSC_EN);
	SET(val, EXYNOS5_PHYCLKRST_REF_SSP_EN);
	SET(val, EXYNOS5_PHYCLKRST_COMMONONN);
	SET(val, EXYNOS5_PHYCLKRST_PORTRESET);
	bus_space_write_4(sc->sc.iot, sc->ph_ioh, EXYNOS5_PHYCLKRST, val);
	delay(10);
	CLR(val, EXYNOS5_PHYCLKRST_PORTRESET);
	bus_space_write_4(sc->sc.iot, sc->ph_ioh, EXYNOS5_PHYCLKRST, val);
}
