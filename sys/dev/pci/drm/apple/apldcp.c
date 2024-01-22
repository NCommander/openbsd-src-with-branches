/*	$OpenBSD$	*/
/*
 * Copyright (c) 2023 Mark Kettenis <kettenis@openbsd.org>
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
#include <sys/device.h>

#include <machine/intr.h>
#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/fdt.h>
#include <dev/ofw/ofw_power.h>
#include <dev/ofw/ofw_clock.h>

static const void *of_device_get_match_data(const struct device *);

#include "dcp.c"

struct apldcp_softc {
	struct platform_device	sc_dev;
};

int	apldcp_match(struct device *, void *, void *);
void	apldcp_attach(struct device *, struct device *, void *);
int	apldcp_activate(struct device *, int);

const struct cfattach apldcp_ca = {
	sizeof (struct apldcp_softc), apldcp_match, apldcp_attach,
	NULL, apldcp_activate
};

struct cfdriver apldcp_cd = {
	NULL, "apldcp", DV_DULL
};

int
apldcp_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "apple,dcp") ||
	    OF_is_compatible(faa->fa_node, "apple,dcpext");
}

void
apldcp_attach(struct device *parent, struct device *self, void *aux)
{
	struct apldcp_softc *sc = (struct apldcp_softc *)self;
	struct fdt_attach_args *faa = aux;

	power_domain_enable(faa->fa_node);
	reset_deassert_all(faa->fa_node);

	printf("\n");

	sc->sc_dev.faa = faa;
	platform_device_register(&sc->sc_dev);

	dcp_platform_probe(&sc->sc_dev);
}

int
apldcp_activate(struct device *self, int act)
{
	int rv;

	switch (act) {
	case DVACT_QUIESCE:
		rv = config_activate_children(self, act);
		dcp_platform_suspend(self);
		break;
	case DVACT_WAKEUP:
		dcp_platform_resume(self);
		rv = config_activate_children(self, act);
		break;
	default:
		rv = config_activate_children(self, act);
		break;
	}

	return rv;
}

/*
 * Linux RTKit interfaces.
 */

#include <arm64/dev/rtkit.h>

struct apple_rtkit_ep {
	struct apple_rtkit *rtk;
	uint8_t ep;

	struct task task;
	uint64_t msg;
};

struct apple_rtkit {
	struct rtkit_state *state;
	struct apple_rtkit_ep ep[64];
	void *cookie;
	struct platform_device *pdev;
	const struct apple_rtkit_ops *ops;
	struct taskq *tq;
};

paddr_t
apple_rtkit_logmap(void *cookie, bus_addr_t addr)
{
	struct apple_rtkit *rtk = cookie;
	int idx, len, node;
	uint32_t *phandles;
	uint32_t iommu_addresses[5];
	bus_addr_t start;
	bus_size_t size;
	uint64_t reg[2];

	len = OF_getproplen(rtk->pdev->node, "memory-region");
	idx = OF_getindex(rtk->pdev->node, "dcp_data", "memory-region-names");
	if (idx < 0 || idx >= len / sizeof(uint32_t))
		return addr;

	phandles = malloc(len, M_TEMP, M_WAITOK | M_ZERO);
	OF_getpropintarray(rtk->pdev->node, "memory-region",
	    phandles, len);
	node = OF_getnodebyphandle(phandles[idx]);
	free(phandles, M_TEMP, len);

	if (node == 0)
		return addr;

	if (!OF_is_compatible(node, "apple,asc-mem"))
		return addr;

	if (OF_getpropint64array(node, "reg", reg, sizeof(reg)) != sizeof(reg))
		return addr;

	if (OF_getpropintarray(node, "iommu-addresses", iommu_addresses,
	    sizeof(iommu_addresses)) < sizeof(iommu_addresses))
		return addr;
	start = (uint64_t)iommu_addresses[1] << 32 | iommu_addresses[2];
	size = (uint64_t)iommu_addresses[3] << 32 | iommu_addresses[4];
	if (addr >= start && addr < start + size)
		return reg[0] + (addr - start);

	/* XXX some machines have truncated DVAs in "iommu-addresses" */
	addr &= 0xffffffff;
	if (addr >= start && addr < start + size)
		return reg[0] + (addr - start);

	return (paddr_t)-1;
}

void
apple_rtkit_do_recv(void *arg)
{
	struct apple_rtkit_ep *rtkep = arg;
	struct apple_rtkit *rtk = rtkep->rtk;

	rtk->ops->recv_message(rtk->cookie, rtkep->ep, rtkep->msg);
}

void
apple_rtkit_recv(void *cookie, uint64_t msg)
{
	struct apple_rtkit_ep *rtkep = cookie;
	struct apple_rtkit *rtk = rtkep->rtk;

	rtkep->msg = msg;
	task_add(rtk->tq, &rtkep->task);
}

int
apple_rtkit_start_ep(struct apple_rtkit *rtk, uint8_t ep)
{
	struct apple_rtkit_ep *rtkep;
	int error;

	rtkep = &rtk->ep[ep];
	rtkep->rtk = rtk;
	rtkep->ep = ep;
	task_set(&rtkep->task, apple_rtkit_do_recv, rtkep);

	error = rtkit_start_endpoint(rtk->state, ep, apple_rtkit_recv, rtkep);
	return -error;
}

int
apple_rtkit_send_message(struct apple_rtkit *rtk, uint8_t ep, uint64_t msg,
			 struct completion *completion, int atomic)
{
	int error;

	error = rtkit_send_endpoint(rtk->state, ep, msg);
	return -error;
}

int
apple_rtkit_wake(struct apple_rtkit *rtk)
{
	int error;

	error = rtkit_set_iop_pwrstate(rtk->state, RTKIT_MGMT_PWR_STATE_INIT);
	if (error)
		return -error;

	error = rtkit_set_ap_pwrstate(rtk->state, RTKIT_MGMT_PWR_STATE_ON);
	return -error;
}

struct apple_rtkit *
devm_apple_rtkit_init(struct device *dev, void *cookie,
    const char *mbox_name, int mbox_idx, const struct apple_rtkit_ops *ops)
{
	struct platform_device *pdev = (struct platform_device *)dev;
	struct apple_rtkit *rtk;
	struct rtkit *rk;

	rtk = malloc(sizeof(*rtk), M_DEVBUF, M_WAITOK | M_ZERO);
	rtk->tq = taskq_create("drmrtk", 1, IPL_HIGH, 0);
	if (rtk->tq == NULL) {
		free(rtk, M_DEVBUF, sizeof(*rtk));
		return ERR_PTR(ENOMEM);
	}

	rk = malloc(sizeof(*rk), M_DEVBUF, M_WAITOK | M_ZERO);
	rk->rk_cookie = rtk;
	rk->rk_dmat = pdev->dmat;
	rk->rk_logmap = apple_rtkit_logmap;

	rtk->state = rtkit_init(pdev->node, mbox_name, 0, rk);
	rtk->cookie = cookie;
	rtk->pdev = pdev;
	rtk->ops = ops;

	return rtk;
}

static const void *
of_device_get_match_data(const struct device *dev)
{
	struct platform_device *pdev = (struct platform_device *)dev;
	int i;

	for (i = 0; i < nitems(of_match); i++) {
		if (OF_is_compatible(pdev->node, of_match[i].compatible))
			return of_match[i].data;
	}

	return NULL;
}
