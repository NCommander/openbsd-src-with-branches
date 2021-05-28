/*	$OpenBSD: aplns.c,v 1.1 2021/05/28 04:36:33 dlg Exp $ */
/*
 * Copyright (c) 2014, 2021 David Gwynne <dlg@openbsd.org>
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
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/timeout.h>
#include <sys/queue.h>
#include <sys/mutex.h>
#include <sys/pool.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_misc.h>
#include <dev/ofw/fdt.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/ic/nvmereg.h>
#include <dev/ic/nvmevar.h>

#define ANS_MODESEL_REG		0x01304
#define ANS_LINEAR_ASQ_DB	0x2490c
#define ANS_LINEAR_IOSQ_DB	0x24910

#define ANS_NVMMU_NUM		0x28100
#define ANS_NVMMU_BASE_ASQ	0x28108
#define ANS_NVMMU_BASE_IOSQ	0x28110
#define ANS_NVMMU_TCB_INVAL	0x28118
#define ANS_NVMMU_TCB_STAT	0x28120

#define ANS_NVMMU_TCB_SIZE	0x4000
#define ANS_NVMMU_TCB_PITCH	0x80

struct ans_nvmmu_tcb {
	uint8_t		tcb_opcode;
	uint8_t		tcb_flags;
#define ANS_NVMMU_TCB_WRITE		(1 << 0)
#define ANS_NVMMU_TCB_READ		(1 << 1)
	uint8_t		tcb_cid;
	uint8_t		tcb_pad0[1];

	uint32_t	tcb_prpl_len;
	uint8_t		tcb_pad1[16];

	uint64_t	tcb_prp[2];
};

int	aplns_match(struct device *, void *, void *);
void	aplns_attach(struct device *, struct device *, void *);

struct cfattach	aplns_ca = {
	sizeof(struct device),
	aplns_match,
	aplns_attach
};

struct cfdriver aplns_cd = {
	NULL, "aplns", DV_DULL
};

int
aplns_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "apple,nvme-m1"));
}

void
aplns_attach(struct device *parent, struct device *self, void *aux)
{
	struct fdt_attach_args *faa = aux;

	printf("\n");

	config_found(self, faa, NULL);
}

struct nvme_ans_softc {
	struct nvme_softc	 asc_nvme;

	struct nvme_dmamem	*asc_nvmmu;
};

int	nvme_ans_match(struct device *, void *, void *);
void	nvme_ans_attach(struct device *, struct device *, void *);

struct cfattach nvme_ans_ca = {
	sizeof(struct nvme_ans_softc),
	nvme_ans_match,
	nvme_ans_attach,
};

void		nvme_ans_enable(struct nvme_softc *);

int		nvme_ans_q_alloc(struct nvme_softc *,
		    struct nvme_queue *);
void		nvme_ans_q_free(struct nvme_softc *,
		    struct nvme_queue *);

uint32_t	nvme_ans_sq_enter(struct nvme_softc *,
		    struct nvme_queue *, struct nvme_ccb *);
void		nvme_ans_sq_leave(struct nvme_softc *,
		    struct nvme_queue *, struct nvme_ccb *);

void		nvme_ans_cq_done(struct nvme_softc *,
		    struct nvme_queue *, struct nvme_ccb *);

static const struct nvme_ops nvme_ans_ops = {
	.op_enable		= nvme_ans_enable,

	.op_q_alloc		= nvme_ans_q_alloc,
	.op_q_free		= nvme_ans_q_free,

	.op_sq_enter		= nvme_ans_sq_enter,
	.op_sq_leave		= nvme_ans_sq_leave,
	.op_sq_enter_locked	= nvme_ans_sq_enter,
	.op_sq_leave_locked	= nvme_ans_sq_leave,

	.op_cq_done		= nvme_ans_cq_done,
};

int
nvme_ans_match(struct device *parent, void *match, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return (OF_is_compatible(faa->fa_node, "apple,nvme-m1"));
}

void
nvme_ans_attach(struct device *parent, struct device *self, void *aux)
{
	struct nvme_ans_softc *asc = (struct nvme_ans_softc *)self;
	struct nvme_softc *sc = &asc->asc_nvme;
	struct fdt_attach_args *faa = aux;

	if (bus_space_map(faa->fa_iot,
	    faa->fa_reg[0].addr, faa->fa_reg[0].size,
	    0, &sc->sc_ioh) != 0) {
		printf(": unable to map registers\n");
		return;
	}

	sc->sc_ih = fdt_intr_establish(faa->fa_node, IPL_BIO,
	    nvme_intr, sc, sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": unable to establish interrupt\n");
		goto unmap;
	}

	sc->sc_dmat = faa->fa_dmat;
	sc->sc_iot = faa->fa_iot;
	sc->sc_ios = faa->fa_reg[0].size;
	sc->sc_ops = &nvme_ans_ops;


	printf(":");
	if (nvme_attach(sc) != 0) {
		/* error printed by nvme_attach() */
		goto disestablish;
	}

	return;

disestablish:
	fdt_intr_disestablish(sc->sc_ih);
	sc->sc_ih = NULL;

unmap:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, sc->sc_ios);
	sc->sc_ios = 0;
}

int
nvme_ans_q_alloc(struct nvme_softc *sc,
    struct nvme_queue *q)
{
	bus_size_t db = ANS_LINEAR_IOSQ_DB;
	bus_size_t base = ANS_NVMMU_BASE_IOSQ;

	KASSERT(q->q_entries <= (ANS_NVMMU_TCB_SIZE / ANS_NVMMU_TCB_PITCH));

	q->q_nvmmu_dmamem = nvme_dmamem_alloc(sc, ANS_NVMMU_TCB_SIZE);
        if (q->q_nvmmu_dmamem == NULL)
		return (-1);

	memset(NVME_DMA_KVA(q->q_nvmmu_dmamem),
	    0, NVME_DMA_LEN(q->q_nvmmu_dmamem));

	switch (q->q_id) {
	case NVME_IO_Q:
	case NVME_HIB_Q:
		break;
	case NVME_ADMIN_Q:
		db = ANS_LINEAR_ASQ_DB;
		base = ANS_NVMMU_BASE_ASQ;
		break;
	default:
		panic("unsupported queue id %u", q->q_id);
		/* NOTREACHED */
	}

	q->q_sqtdbl = db;

	nvme_dmamem_sync(sc, q->q_nvmmu_dmamem, BUS_DMASYNC_PREWRITE);
	nvme_write8(sc, base, NVME_DMA_DVA(q->q_nvmmu_dmamem));

	return (0);
}

void
nvme_ans_enable(struct nvme_softc *sc)
{
	nvme_write4(sc, ANS_NVMMU_NUM,
	    (ANS_NVMMU_TCB_SIZE / ANS_NVMMU_TCB_PITCH) - 1);
	nvme_write4(sc, ANS_MODESEL_REG, 0);
}

void
nvme_ans_q_free(struct nvme_softc *sc,
    struct nvme_queue *q)
{
        nvme_dmamem_sync(sc, q->q_nvmmu_dmamem, BUS_DMASYNC_POSTWRITE);
	nvme_dmamem_free(sc, q->q_nvmmu_dmamem);
}

uint32_t
nvme_ans_sq_enter(struct nvme_softc *sc,
    struct nvme_queue *q, struct nvme_ccb *ccb)
{
	return (ccb->ccb_id);
}

static inline struct ans_nvmmu_tcb *
nvme_ans_tcb(struct nvme_queue *q, unsigned int qid)
{
	caddr_t ptr = NVME_DMA_KVA(q->q_nvmmu_dmamem);
	ptr += qid * ANS_NVMMU_TCB_PITCH;
	return ((struct ans_nvmmu_tcb *)ptr);
}

void
nvme_ans_sq_leave(struct nvme_softc *sc, 
    struct nvme_queue *q, struct nvme_ccb *ccb)
{
	unsigned int id = ccb->ccb_id;
	struct nvme_sqe_io *sqe;
	struct ans_nvmmu_tcb *tcb = nvme_ans_tcb(q, id);

	sqe = NVME_DMA_KVA(q->q_sq_dmamem);
	sqe += id;

	bus_dmamap_sync(sc->sc_dmat, NVME_DMA_MAP(q->q_nvmmu_dmamem),
	    ANS_NVMMU_TCB_PITCH * id, sizeof(*tcb), BUS_DMASYNC_POSTWRITE);

	memset(tcb, 0, sizeof(*tcb));
	tcb->tcb_opcode = sqe->opcode;
	tcb->tcb_flags = ANS_NVMMU_TCB_WRITE | ANS_NVMMU_TCB_READ;
	tcb->tcb_cid = id;
	tcb->tcb_prpl_len = sqe->nlb;
	tcb->tcb_prp[0] = sqe->entry.prp[0];
	tcb->tcb_prp[1] = sqe->entry.prp[1];

	bus_dmamap_sync(sc->sc_dmat, NVME_DMA_MAP(q->q_nvmmu_dmamem),
	    ANS_NVMMU_TCB_PITCH * id, sizeof(*tcb), BUS_DMASYNC_PREWRITE);

	nvme_write4(sc, q->q_sqtdbl, id);
}

void
nvme_ans_cq_done(struct nvme_softc *sc,
    struct nvme_queue *q, struct nvme_ccb *ccb)
{
	unsigned int id = ccb->ccb_id;
	struct ans_nvmmu_tcb *tcb = nvme_ans_tcb(q, id);
	uint32_t stat;

	bus_dmamap_sync(sc->sc_dmat, NVME_DMA_MAP(q->q_nvmmu_dmamem),
	    ANS_NVMMU_TCB_PITCH * id, sizeof(*tcb), BUS_DMASYNC_POSTWRITE);
	memset(tcb, 0, sizeof(*tcb));
	bus_dmamap_sync(sc->sc_dmat, NVME_DMA_MAP(q->q_nvmmu_dmamem),
	    ANS_NVMMU_TCB_PITCH * id, sizeof(*tcb), BUS_DMASYNC_PREWRITE);

	nvme_write4(sc, ANS_NVMMU_TCB_INVAL, id);
	stat = nvme_read4(sc, ANS_NVMMU_TCB_STAT);
	if (stat != 0) {
		printf("%s: nvmmu tcp stat is non-zero: 0x%08x\n",
		    DEVNAME(sc), stat);
	}
}
