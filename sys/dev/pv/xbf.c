/*	$OpenBSD: xbf.c,v 1.19 2017/02/08 16:29:00 mikeb Exp $	*/

/*
 * Copyright (c) 2016 Mike Belopuhov
 * Copyright (c) 2009, 2011 Mark Kettenis
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

#include "bio.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/atomic.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/task.h>

#include <machine/bus.h>

#include <dev/pv/xenreg.h>
#include <dev/pv/xenvar.h>

#include <scsi/scsi_all.h>
#include <scsi/cd.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

/* #define XBF_DEBUG */

#ifdef XBF_DEBUG
#define DPRINTF(x...)		printf(x)
#else
#define DPRINTF(x...)
#endif

#define XBF_OP_READ		0
#define XBF_OP_WRITE		1
#define XBF_OP_BARRIER		2 /* feature-barrier */
#define XBF_OP_FLUSH		3 /* feature-flush-cache */
#define XBF_OP_DISCARD		5 /* feature-discard */
#define XBF_OP_INDIRECT		6 /* feature-max-indirect-segments */

#define XBF_MAX_SGE		11
#define XBF_MAX_ISGE		8

#define XBF_SEC_SHIFT		9

#define XBF_CDROM		1
#define XBF_REMOVABLE		2
#define XBF_READONLY		4

#define XBF_OK			0
#define XBF_EIO			-1 /* generic failure */
#define XBF_EOPNOTSUPP		-2 /* only for XBF_OP_BARRIER */

struct xbf_sge {
	uint32_t		 sge_ref;
	uint8_t			 sge_first;
	uint8_t			 sge_last;
	uint16_t		 sge_pad;
} __packed;

/* Generic I/O request */
struct xbf_req {
	uint8_t			 req_op;
	uint8_t			 req_nsegs;
	uint16_t		 req_unit;
#ifdef __amd64__
	uint32_t		 req_pad;
#endif
	uint64_t		 req_id;
	uint64_t		 req_sector;
	struct xbf_sge		 req_sgl[XBF_MAX_SGE];
} __packed;

/* Indirect I/O request */
struct xbf_ireq {
	uint8_t			 req_op;
	uint8_t			 req_iop;
	uint16_t		 req_nsegs;
#ifdef __amd64__
	uint32_t		 req_pad;
#endif
	uint64_t		 req_id;
	uint64_t		 req_sector;
	uint16_t		 req_unit;
	uint32_t		 req_gref[XBF_MAX_ISGE];
#ifdef __i386__
	uint64_t		 req_pad;
#endif
} __packed;

struct xbf_rsp {
	uint64_t		 rsp_id;
	uint8_t			 rsp_op;
	uint8_t			 rsp_pad1;
	int16_t			 rsp_status;
#ifdef __amd64__
	uint32_t		 rsp_pad2;
#endif
} __packed;

union xbf_ring_desc {
	struct xbf_req	 	 xrd_req;
	struct xbf_ireq		 xrd_ireq;
	struct xbf_rsp	 	 xrd_rsp;
} __packed;

#define XBF_MIN_RING_SIZE	1
#define XBF_MAX_RING_SIZE	8
#define XBF_MAX_REQS		256 /* must be a power of 2 */

struct xbf_ring {
	volatile uint32_t	 xr_prod;
	volatile uint32_t	 xr_prod_event;
	volatile uint32_t	 xr_cons;
	volatile uint32_t	 xr_cons_event;
	uint32_t		 xr_reserved[12];
	union xbf_ring_desc	 xr_desc[0];
} __packed;

struct xbf_dma_mem {
	bus_size_t		 dma_size;
	bus_dma_tag_t		 dma_tag;
	bus_dmamap_t		 dma_map;
	bus_dma_segment_t	*dma_seg;
	int			 dma_nsegs; /* total amount */
	int			 dma_rsegs; /* used amount */
	caddr_t			 dma_vaddr;
};

struct xbf_softc {
	struct device		 sc_dev;
	struct device		*sc_parent;
	char			 sc_node[XEN_MAX_NODE_LEN];
	char			 sc_backend[XEN_MAX_BACKEND_LEN];
	bus_dma_tag_t		 sc_dmat;
	int			 sc_domid;

	xen_intr_handle_t	 sc_xih;

	int			 sc_state;
#define  XBF_CONNECTED		  4
#define  XBF_CLOSING		  5

	int			 sc_caps;
#define  XBF_CAP_BARRIER	  0x0001
#define  XBF_CAP_FLUSH		  0x0002

	uint32_t		 sc_type;
	uint32_t		 sc_unit;
	char			 sc_dtype[16];
	char			 sc_prod[16];

	uint32_t		 sc_maxphys;
	uint64_t		 sc_disk_size;
	uint32_t		 sc_block_size;

	struct xbf_ring		*sc_xr;
	uint32_t		 sc_xr_cons;
	uint32_t		 sc_xr_prod;
	uint32_t		 sc_xr_size; /* in pages */
	struct xbf_dma_mem	 sc_xr_dma;
	uint32_t		 sc_xr_ref[XBF_MAX_RING_SIZE];
	int			 sc_xr_ndesc;

	struct scsi_xfer	**sc_xs;
	bus_dmamap_t		*sc_xs_map;
	int			 sc_xs_avail;
	struct xbf_dma_mem	*sc_xs_bb;

	struct scsi_iopool	 sc_iopool;
	struct scsi_adapter	 sc_switch;
	struct scsi_link         sc_link;
	struct device		*sc_scsibus;
};

int	xbf_match(struct device *, void *, void *);
void	xbf_attach(struct device *, struct device *, void *);
int	xbf_detach(struct device *, int);

struct cfdriver xbf_cd = {
	NULL, "xbf", DV_DULL
};

const struct cfattach xbf_ca = {
	sizeof(struct xbf_softc), xbf_match, xbf_attach, xbf_detach
};

void	xbf_intr(void *);

void	*xbf_io_get(void *);
void	xbf_io_put(void *, void *);

int	xbf_load_xs(struct scsi_xfer *, int);
int	xbf_bounce_xs(struct scsi_xfer *, int);
void	xbf_reclaim_xs(struct scsi_xfer *, int);

void	xbf_scsi_cmd(struct scsi_xfer *);
int	xbf_submit_cmd(struct scsi_xfer *);
int	xbf_poll_cmd(struct scsi_xfer *, int, int);
void	xbf_complete_cmd(struct scsi_xfer *, int);
int	xbf_dev_probe(struct scsi_link *);

void	xbf_scsi_minphys(struct buf *, struct scsi_link *);
void	xbf_scsi_inq(struct scsi_xfer *);
void	xbf_scsi_inquiry(struct scsi_xfer *);
void	xbf_scsi_capacity(struct scsi_xfer *);
void	xbf_scsi_capacity16(struct scsi_xfer *);
void	xbf_scsi_done(struct scsi_xfer *, int);

int	xbf_dma_alloc(struct xbf_softc *, struct xbf_dma_mem *,
	    bus_size_t, int, int);
void	xbf_dma_free(struct xbf_softc *, struct xbf_dma_mem *);

int	xbf_get_type(struct xbf_softc *);
int	xbf_init(struct xbf_softc *);
int	xbf_ring_create(struct xbf_softc *);
void	xbf_ring_destroy(struct xbf_softc *);
void	xbf_stop(struct xbf_softc *);

int
xbf_match(struct device *parent, void *match, void *aux)
{
	struct xen_attach_args *xa = aux;

	if (strcmp("vbd", xa->xa_name))
		return (0);

	return (1);
}

void
xbf_attach(struct device *parent, struct device *self, void *aux)
{
	struct xen_attach_args *xa = aux;
	struct xbf_softc *sc = (struct xbf_softc *)self;
	struct scsibus_attach_args saa;

	sc->sc_parent = parent;
	sc->sc_dmat = xa->xa_dmat;
	sc->sc_domid = xa->xa_domid;

	memcpy(sc->sc_node, xa->xa_node, XEN_MAX_NODE_LEN);
	memcpy(sc->sc_backend, xa->xa_backend, XEN_MAX_BACKEND_LEN);

	if (xbf_get_type(sc))
		return;

	if (xen_intr_establish(0, &sc->sc_xih, sc->sc_domid, xbf_intr, sc,
	    sc->sc_dev.dv_xname)) {
		printf(": failed to establish an interrupt\n");
		return;
	}
	xen_intr_mask(sc->sc_xih);

	printf(" backend %d chan %u: %s\n", sc->sc_domid, sc->sc_xih,
	    sc->sc_dtype);

	scsi_iopool_init(&sc->sc_iopool, sc, xbf_io_get, xbf_io_put);

	if (xbf_init(sc))
		goto error;

	if (xen_intr_unmask(sc->sc_xih)) {
		printf("%s: failed to enable interrupts\n",
		    sc->sc_dev.dv_xname);
		goto error;
	}

	sc->sc_switch.scsi_cmd = xbf_scsi_cmd;
	sc->sc_switch.scsi_minphys = xbf_scsi_minphys;
	sc->sc_switch.dev_probe = xbf_dev_probe;

	sc->sc_link.adapter = &sc->sc_switch;
	sc->sc_link.adapter_softc = self;
	sc->sc_link.adapter_buswidth = 2;
	sc->sc_link.luns = 1;
	sc->sc_link.adapter_target = 2;
	sc->sc_link.openings = sc->sc_xr_ndesc - 1;
	sc->sc_link.pool = &sc->sc_iopool;

	bzero(&saa, sizeof(saa));
	saa.saa_sc_link = &sc->sc_link;
	sc->sc_scsibus = config_found(self, &saa, scsiprint);

	xen_unplug_emulated(parent, XEN_UNPLUG_IDE | XEN_UNPLUG_IDESEC);

	return;

 error:
	xen_intr_disestablish(sc->sc_xih);
}

int
xbf_detach(struct device *self, int flags)
{
	struct xbf_softc *sc = (struct xbf_softc *)self;
	int ostate = sc->sc_state;

	sc->sc_state = XBF_CLOSING;

	xen_intr_mask(sc->sc_xih);
	xen_intr_barrier(sc->sc_xih);

	if (ostate == XBF_CONNECTED) {
		xen_intr_disestablish(sc->sc_xih);
		xbf_stop(sc);
	}

	if (sc->sc_scsibus)
		return (config_detach(sc->sc_scsibus, flags | DETACH_FORCE));

	return (0);
}

void
xbf_intr(void *xsc)
{
	struct xbf_softc *sc = xsc;
	struct xbf_ring *xr = sc->sc_xr;
	struct xbf_dma_mem *dma = &sc->sc_xr_dma;
	struct scsi_xfer *xs;
	uint32_t cons;
	int desc;

	bus_dmamap_sync(dma->dma_tag, dma->dma_map, 0, dma->dma_size,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	for (cons = sc->sc_xr_cons; cons != xr->xr_cons; cons++) {
		desc = cons & (sc->sc_xr_ndesc - 1);
		xs = sc->sc_xs[desc];
		if (xs != NULL)
			xbf_complete_cmd(xs, desc);
	}

	sc->sc_xr_cons = cons;
}

void *
xbf_io_get(void *xsc)
{
	struct xbf_softc *sc = xsc;
	void *rv = sc; /* just has to be !NULL */

	if (sc->sc_state != XBF_CONNECTED &&
	    sc->sc_state != XBF_CLOSING)
		rv = NULL;

	return (rv);
}

void
xbf_io_put(void *xsc, void *io)
{
#ifdef DIAGNOSTIC
	struct xbf_softc *sc = xsc;

	if (sc != io)
		panic("xbf_io_put: unexpected io");
#endif
}

void
xbf_scsi_cmd(struct scsi_xfer *xs)
{
	struct xbf_softc *sc = xs->sc_link->adapter_softc;
	int desc;

	switch (xs->cmd->opcode) {
	case READ_BIG:
	case READ_COMMAND:
	case READ_12:
	case READ_16:
	case WRITE_BIG:
	case WRITE_COMMAND:
	case WRITE_12:
	case WRITE_16:
		if (sc->sc_state != XBF_CONNECTED) {
			xbf_scsi_done(xs, XS_SELTIMEOUT);
			return;
		}
		break;
	case SYNCHRONIZE_CACHE:
		if (!(sc->sc_caps & (XBF_CAP_BARRIER|XBF_CAP_FLUSH))) {
			xbf_scsi_done(xs, XS_NOERROR);
			return;
		}
		break;
	case INQUIRY:
		xbf_scsi_inq(xs);
		return;
	case READ_CAPACITY:
		xbf_scsi_capacity(xs);
		return;
	case READ_CAPACITY_16:
		xbf_scsi_capacity16(xs);
		return;
	case TEST_UNIT_READY:
	case START_STOP:
	case PREVENT_ALLOW:
		xbf_scsi_done(xs, XS_NOERROR);
		return;
	default:
		printf("%s cmd 0x%02x\n", __func__, xs->cmd->opcode);
	case MODE_SENSE:
	case MODE_SENSE_BIG:
	case REPORT_LUNS:
	case READ_TOC:
		xbf_scsi_done(xs, XS_DRIVER_STUFFUP);
		return;
	}

	desc = xbf_submit_cmd(xs);
	if (desc < 0) {
		xbf_scsi_done(xs, XS_DRIVER_STUFFUP);
		return;
	}

	if (ISSET(xs->flags, SCSI_POLL) && xbf_poll_cmd(xs, desc, 1000)) {
		DPRINTF("%s: desc %u timed out\n", sc->sc_dev.dv_xname, desc);
		sc->sc_xs[desc] = NULL;
		xbf_reclaim_xs(xs, desc);
		xbf_scsi_done(xs, XS_TIMEOUT);
		return;
	}
}

int
xbf_load_xs(struct scsi_xfer *xs, int desc)
{
	struct xbf_softc *sc = xs->sc_link->adapter_softc;
	struct xbf_sge *sge;
	union xbf_ring_desc *xrd;
	bus_dmamap_t map;
	int i, error, mapflags;

	xrd = &sc->sc_xr->xr_desc[desc];
	map = sc->sc_xs_map[desc];

	mapflags = (sc->sc_domid << 16);
	if (ISSET(xs->flags, SCSI_NOSLEEP))
		mapflags |= BUS_DMA_NOWAIT;
	else
		mapflags |= BUS_DMA_WAITOK;
	if (ISSET(xs->flags, SCSI_DATA_IN))
		mapflags |= BUS_DMA_READ;
	else
		mapflags |= BUS_DMA_WRITE;

	error = bus_dmamap_load(sc->sc_dmat, map, xs->data, xs->datalen,
	    NULL, mapflags);
	if (error) {
		DPRINTF("%s: failed to load %u bytes of data\n",
		    sc->sc_dev.dv_xname, xs->datalen);
		return (-1);
	}

	for (i = 0; i < map->dm_nsegs; i++) {
		sge = &xrd->xrd_req.req_sgl[i];
		sge->sge_ref = map->dm_segs[i].ds_addr;
		sge->sge_first = i > 0 ? 0 :
		    ((vaddr_t)xs->data & PAGE_MASK) >> XBF_SEC_SHIFT;
		sge->sge_last = sge->sge_first +
		    (map->dm_segs[i].ds_len >> XBF_SEC_SHIFT) - 1;

		DPRINTF("%s:   seg %d/%d ref %lu len %lu first %u last %u\n",
		    sc->sc_dev.dv_xname, i + 1, map->dm_nsegs,
		    map->dm_segs[i].ds_addr, map->dm_segs[i].ds_len,
		    sge->sge_first, sge->sge_last);

		KASSERT(sge->sge_last <= 7);
	}

	xrd->xrd_req.req_nsegs = map->dm_nsegs;

	return (0);
}

int
xbf_bounce_xs(struct scsi_xfer *xs, int desc)
{
	struct xbf_softc *sc = xs->sc_link->adapter_softc;
	struct xbf_sge *sge;
	struct xbf_dma_mem *dma;
	union xbf_ring_desc *xrd;
	bus_dmamap_t map;
	bus_size_t size;
	int i, error, mapflags;

	xrd = &sc->sc_xr->xr_desc[desc];
	dma = &sc->sc_xs_bb[desc];

	size = roundup(xs->datalen, PAGE_SIZE);
	if (size > sc->sc_maxphys)
		return (EFBIG);

	mapflags = (sc->sc_domid << 16);
	if (ISSET(xs->flags, SCSI_NOSLEEP))
		mapflags |= BUS_DMA_NOWAIT;
	else
		mapflags |= BUS_DMA_WAITOK;

	error = xbf_dma_alloc(sc, dma, size, size / PAGE_SIZE, mapflags);
	if (error) {
		DPRINTF("%s: failed to allocate a %lu byte bounce buffer\n",
		    sc->sc_dev.dv_xname, size);
		return (error);
	}

	map = dma->dma_map;

	DPRINTF("%s: bouncing %d bytes via %ld size map with %d segments\n",
	    sc->sc_dev.dv_xname, xs->datalen, size, map->dm_nsegs);

	if (ISSET(xs->flags, SCSI_DATA_OUT))
		memcpy((caddr_t)dma->dma_vaddr, xs->data, xs->datalen);

	for (i = 0; i < map->dm_nsegs; i++) {
		sge = &xrd->xrd_req.req_sgl[i];
		sge->sge_ref = map->dm_segs[i].ds_addr;
		sge->sge_first = i > 0 ? 0 :
		    ((vaddr_t)xs->data & PAGE_MASK) >> XBF_SEC_SHIFT;
		sge->sge_last = sge->sge_first +
		    (map->dm_segs[i].ds_len >> XBF_SEC_SHIFT) - 1;

		DPRINTF("%s:   seg %d/%d ref %lu len %lu first %u last %u\n",
		    sc->sc_dev.dv_xname, i + 1, map->dm_nsegs,
		    map->dm_segs[i].ds_addr, map->dm_segs[i].ds_len,
		    sge->sge_first, sge->sge_last);

		KASSERT(sge->sge_last <= 7);
	}

	xrd->xrd_req.req_nsegs = map->dm_nsegs;

	return (0);
}

void
xbf_reclaim_xs(struct scsi_xfer *xs, int desc)
{
	struct xbf_softc *sc = xs->sc_link->adapter_softc;
	struct xbf_dma_mem *dma;

	dma = &sc->sc_xs_bb[desc];
	if (dma->dma_size == 0)
		return;

	if (ISSET(xs->flags, SCSI_DATA_IN))
		memcpy(xs->data, (caddr_t)dma->dma_vaddr, xs->datalen);

	xbf_dma_free(sc, dma);
}

int
xbf_submit_cmd(struct scsi_xfer *xs)
{
	struct xbf_softc *sc = xs->sc_link->adapter_softc;
	union xbf_ring_desc *xrd;
	bus_dmamap_t map;
	struct scsi_rw *rw;
	struct scsi_rw_big *rwb;
	struct scsi_rw_12 *rw12;
	struct scsi_rw_16 *rw16;
	uint64_t lba = 0;
	uint32_t nblk = 0;
	uint8_t operation = 0;
	int desc, error;

	switch (xs->cmd->opcode) {
	case READ_BIG:
	case READ_COMMAND:
	case READ_12:
	case READ_16:
		operation = XBF_OP_READ;
		break;

	case WRITE_BIG:
	case WRITE_COMMAND:
	case WRITE_12:
	case WRITE_16:
		operation = XBF_OP_WRITE;
		break;

	case SYNCHRONIZE_CACHE:
		if (sc->sc_caps & XBF_CAP_FLUSH)
			operation = XBF_OP_FLUSH;
		else if (sc->sc_caps & XBF_CAP_BARRIER)
			operation = XBF_OP_BARRIER;
		break;
	}

	/*
	 * READ/WRITE/SYNCHRONIZE commands. SYNCHRONIZE CACHE
	 * has the same layout as 10-byte READ/WRITE commands.
	 */
	if (xs->cmdlen == 6) {
		rw = (struct scsi_rw *)xs->cmd;
		lba = _3btol(rw->addr) & (SRW_TOPADDR << 16 | 0xffff);
		nblk = rw->length ? rw->length : 0x100;
	} else if (xs->cmdlen == 10) {
		rwb = (struct scsi_rw_big *)xs->cmd;
		lba = _4btol(rwb->addr);
		nblk = _2btol(rwb->length);
	} else if (xs->cmdlen == 12) {
		rw12 = (struct scsi_rw_12 *)xs->cmd;
		lba = _4btol(rw12->addr);
		nblk = _4btol(rw12->length);
	} else if (xs->cmdlen == 16) {
		rw16 = (struct scsi_rw_16 *)xs->cmd;
		lba = _8btol(rw16->addr);
		nblk = _4btol(rw16->length);
	}

	desc = sc->sc_xr_prod & (sc->sc_xr_ndesc - 1);
	xrd = &sc->sc_xr->xr_desc[desc];
	map = sc->sc_xs_map[desc];

	xrd->xrd_req.req_op = operation;
	xrd->xrd_req.req_unit = (uint16_t)sc->sc_unit;
	xrd->xrd_req.req_sector = lba;

	if (operation == XBF_OP_READ || operation == XBF_OP_WRITE) {
		DPRINTF("%s: desc %u %s%s lba %llu nsec %u len %u\n",
		    sc->sc_dev.dv_xname, desc, operation == XBF_OP_READ ?
		    "read" : "write", ISSET(xs->flags, SCSI_POLL) ? "-poll" :
		    "", lba, nblk, xs->datalen);

		if (((vaddr_t)xs->data & ((1 << XBF_SEC_SHIFT) - 1)) == 0)
			error = xbf_load_xs(xs, desc);
		else
			error = xbf_bounce_xs(xs, desc);
		if (error)
			return (error);
	} else {
		DPRINTF("%s: desc %u %s%s lba %llu\n", sc->sc_dev.dv_xname,
		    desc, operation == XBF_OP_FLUSH ? "flush" : "barrier",
		    ISSET(xs->flags, SCSI_POLL) ? "-poll" : "", lba);
		xrd->xrd_req.req_nsegs = 0;
	}

	sc->sc_xs[desc] = xs;

	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	sc->sc_xr_prod++;
	sc->sc_xr->xr_prod = sc->sc_xr_prod;
	sc->sc_xr->xr_cons_event = sc->sc_xr_prod;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_xr_dma.dma_map, 0,
	    sc->sc_xr_dma.dma_map->dm_mapsize, BUS_DMASYNC_PREREAD |
	    BUS_DMASYNC_PREWRITE);

	xen_intr_signal(sc->sc_xih);

	return desc;
}

int
xbf_poll_cmd(struct scsi_xfer *xs, int desc, int timo)
{
	do {
		if (ISSET(xs->flags, ITSDONE))
			break;
		if (ISSET(xs->flags, SCSI_NOSLEEP))
			delay(10);
		else
			tsleep(xs, PRIBIO, "xbfpoll", 1);
	} while(--timo > 0);

	return (0);
}

void
xbf_complete_cmd(struct scsi_xfer *xs, int desc)
{
	struct xbf_softc *sc = xs->sc_link->adapter_softc;
	union xbf_ring_desc *xrd;
	bus_dmamap_t map;
	uint64_t id;
	int error;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_xr_dma.dma_map, 0,
	    sc->sc_xr_dma.dma_map->dm_mapsize, BUS_DMASYNC_POSTREAD |
	    BUS_DMASYNC_POSTWRITE);

	xrd = &sc->sc_xr->xr_desc[desc];
	error = xrd->xrd_rsp.rsp_status == XBF_OK ? XS_NOERROR :
	    XS_DRIVER_STUFFUP;

	if (sc->sc_xs_bb[desc].dma_size > 0)
		map = sc->sc_xs_bb[desc].dma_map;
	else
		map = sc->sc_xs_map[desc];
	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, map);

	sc->sc_xs[desc] = NULL;

	DPRINTF("%s: completing desc %u(%llu) op %u with error %d\n",
	    sc->sc_dev.dv_xname, desc, xrd->xrd_rsp.rsp_id,
	    xrd->xrd_rsp.rsp_op, xrd->xrd_rsp.rsp_status);

	id = xrd->xrd_rsp.rsp_id;
	memset(xrd, 0, sizeof(*xrd));
	xrd->xrd_req.req_id = id;

	xs->resid = 0;

	xbf_reclaim_xs(xs, desc);
	xbf_scsi_done(xs, error);
}

void
xbf_scsi_minphys(struct buf *bp, struct scsi_link *sl)
{
	struct xbf_softc *sc = sl->adapter_softc;

	if (bp->b_bcount > sc->sc_maxphys)
		bp->b_bcount = sc->sc_maxphys;
}

void
xbf_scsi_inq(struct scsi_xfer *xs)
{
	struct scsi_inquiry *inq = (struct scsi_inquiry *)xs->cmd;

	if (ISSET(inq->flags, SI_EVPD))
		xbf_scsi_done(xs, XS_DRIVER_STUFFUP);
	else
		xbf_scsi_inquiry(xs);
}

void
xbf_scsi_inquiry(struct scsi_xfer *xs)
{
	struct xbf_softc *sc = xs->sc_link->adapter_softc;
	struct scsi_inquiry_data inq;
	/* char buf[5]; */

	bzero(&inq, sizeof(inq));

	switch (sc->sc_type) {
	case XBF_CDROM:
		inq.device = T_CDROM;
		break;
	default:
		inq.device = T_DIRECT;
		break;
	}

	inq.version = 0x05; /* SPC-3 */
	inq.response_format = 2;
	inq.additional_length = 32;
	inq.flags |= SID_CmdQue;
	bcopy("Xen     ", inq.vendor, sizeof(inq.vendor));
	bcopy(sc->sc_prod, inq.product, sizeof(inq.product));
	bcopy("0000", inq.revision, sizeof(inq.revision));

	bcopy(&inq, xs->data, MIN(sizeof(inq), xs->datalen));

	xbf_scsi_done(xs, XS_NOERROR);
}

void
xbf_scsi_capacity(struct scsi_xfer *xs)
{
	struct xbf_softc *sc = xs->sc_link->adapter_softc;
	struct scsi_read_cap_data rcd;
	uint64_t capacity;

	bzero(&rcd, sizeof(rcd));

	capacity = sc->sc_disk_size - 1;
	if (capacity > 0xffffffff)
		capacity = 0xffffffff;

	_lto4b(capacity, rcd.addr);
	_lto4b(sc->sc_block_size, rcd.length);

	bcopy(&rcd, xs->data, MIN(sizeof(rcd), xs->datalen));

	xbf_scsi_done(xs, XS_NOERROR);
}

void
xbf_scsi_capacity16(struct scsi_xfer *xs)
{
	struct xbf_softc *sc = xs->sc_link->adapter_softc;
	struct scsi_read_cap_data_16 rcd;

	bzero(&rcd, sizeof(rcd));

	_lto8b(sc->sc_disk_size - 1, rcd.addr);
	_lto4b(sc->sc_block_size, rcd.length);

	bcopy(&rcd, xs->data, MIN(sizeof(rcd), xs->datalen));

	xbf_scsi_done(xs, XS_NOERROR);
}

void
xbf_scsi_done(struct scsi_xfer *xs, int error)
{
	int s;

	xs->error = error;

	s = splbio();
	scsi_done(xs);
	splx(s);
}

int
xbf_dev_probe(struct scsi_link *link)
{
	KASSERT(link->lun == 0);

	if (link->target == 0)
		return (0);

	return (ENODEV);
}

int
xbf_get_type(struct xbf_softc *sc)
{
	unsigned long long res;
	const char *prop;
	char val[32];
	int error;

	prop = "type";
	if ((error = xs_getprop(sc->sc_parent, sc->sc_backend, prop, val,
	    sizeof(val))) != 0)
		goto errout;
	snprintf(sc->sc_prod, sizeof(sc->sc_prod), "%s", val);

	prop = "dev";
	if ((error = xs_getprop(sc->sc_parent, sc->sc_backend, prop, val,
	    sizeof(val))) != 0)
		goto errout;
	snprintf(sc->sc_prod, sizeof(sc->sc_prod), "%s %s", sc->sc_prod, val);

	prop = "virtual-device";
	if ((error = xs_getnum(sc->sc_parent, sc->sc_node, prop, &res)) != 0)
		goto errout;
	sc->sc_unit = (uint32_t)res;
	snprintf(sc->sc_prod, sizeof(sc->sc_prod), "%s %llu", sc->sc_prod, res);

	prop = "device-type";
	if ((error = xs_getprop(sc->sc_parent, sc->sc_node, prop,
	    sc->sc_dtype, sizeof(sc->sc_dtype))) != 0)
		goto errout;
	if (!strcmp(sc->sc_dtype, "cdrom"))
		sc->sc_type = XBF_CDROM;

	return (0);

 errout:
	printf("%s: failed to read \"%s\" property\n", sc->sc_dev.dv_xname,
	    prop);
	return (-1);
}

int
xbf_init(struct xbf_softc *sc)
{
	unsigned long long res;
	const char *action, *prop;
	char pbuf[sizeof("ring-refXX")];
	int i, error;

	prop = "max-ring-page-order";
	error = xs_getnum(sc->sc_parent, sc->sc_backend, prop, &res);
	if (error == 0)
		sc->sc_xr_size = 1 << res;
	if (error == ENOENT) {
		prop = "max-ring-pages";
		error = xs_getnum(sc->sc_parent, sc->sc_backend, prop, &res);
		if (error == 0)
			sc->sc_xr_size = res;
	}
	/* Fallback to the known minimum */
	if (error)
		sc->sc_xr_size = XBF_MIN_RING_SIZE;

	if (sc->sc_xr_size < XBF_MIN_RING_SIZE)
		sc->sc_xr_size = XBF_MIN_RING_SIZE;
	if (sc->sc_xr_size > XBF_MAX_RING_SIZE)
		sc->sc_xr_size = XBF_MAX_RING_SIZE;
	if (!powerof2(sc->sc_xr_size))
		sc->sc_xr_size = 1 << (fls(sc->sc_xr_size) - 1);

	sc->sc_xr_ndesc = ((sc->sc_xr_size * PAGE_SIZE) -
	    sizeof(struct xbf_ring)) / sizeof(union xbf_ring_desc);
	if (!powerof2(sc->sc_xr_ndesc))
		sc->sc_xr_ndesc = 1 << (fls(sc->sc_xr_ndesc) - 1);
	if (sc->sc_xr_ndesc > XBF_MAX_REQS)
		sc->sc_xr_ndesc = XBF_MAX_REQS;

	DPRINTF("%s: %u ring pages, %u requests\n",
	    sc->sc_dev.dv_xname, sc->sc_xr_size, sc->sc_xr_ndesc);

	if (xbf_ring_create(sc))
		return (-1);

	action = "set";

	for (i = 0; i < sc->sc_xr_size; i++) {
		if (i == 0 && sc->sc_xr_size == 1)
			snprintf(pbuf, sizeof(pbuf), "ring-ref");
		else
			snprintf(pbuf, sizeof(pbuf), "ring-ref%u", i);
		prop = pbuf;
		if (xs_setnum(sc->sc_parent, sc->sc_node, prop,
		    sc->sc_xr_ref[i]))
			goto errout;
	}

	if (sc->sc_xr_size > 1) {
		prop = "num-ring-pages";
		if (xs_setnum(sc->sc_parent, sc->sc_node, prop,
		    sc->sc_xr_size))
			goto errout;
		prop = "ring-page-order";
		if (xs_setnum(sc->sc_parent, sc->sc_node, prop,
		    fls(sc->sc_xr_size) - 1))
			goto errout;
	}

	prop = "event-channel";
	if (xs_setnum(sc->sc_parent, sc->sc_node, prop, sc->sc_xih))
		goto errout;

	prop = "protocol";
#ifdef __amd64__
	if (xs_setprop(sc->sc_parent, sc->sc_node, prop, "x86_64-abi",
	    strlen("x86_64-abi")))
		goto errout;
#else
	if (xs_setprop(sc->sc_parent, sc->sc_node, prop, "x86_32-abi",
	    strlen("x86_32-abi")))
		goto errout;
#endif

	if (xs_setprop(sc->sc_parent, sc->sc_node, "state",
	    XEN_STATE_INITIALIZED, strlen(XEN_STATE_INITIALIZED))) {
		printf("%s: failed to set state to INITIALIZED\n",
		    sc->sc_dev.dv_xname);
		xbf_ring_destroy(sc);
		return (-1);
	}

	if (xs_await_transition(sc->sc_parent, sc->sc_backend, "state",
	    XEN_STATE_CONNECTED, 10000)) {
		printf("%s: timed out waiting for backend to connect\n",
		    sc->sc_dev.dv_xname);
		xbf_ring_destroy(sc);
		return (-1);
	}

	action = "read";

	prop = "sectors";
	if ((error = xs_getnum(sc->sc_parent, sc->sc_backend, prop, &res)) != 0)
		goto errout;
	sc->sc_disk_size = res;

	prop = "sector-size";
	if ((error = xs_getnum(sc->sc_parent, sc->sc_backend, prop, &res)) != 0)
		goto errout;
	sc->sc_block_size = res;

	prop = "feature-barrier";
	if ((error = xs_getnum(sc->sc_parent, sc->sc_backend, prop, &res)) != 0
	    && error != ENOENT)
		goto errout;
	if (error == 0 && res == 1)
		sc->sc_caps |= XBF_CAP_BARRIER;

	prop = "feature-flush-cache";
	if ((error = xs_getnum(sc->sc_parent, sc->sc_backend, prop, &res)) != 0
	    && error != ENOENT)
		goto errout;
	if (error == 0 && res == 1)
		sc->sc_caps |= XBF_CAP_FLUSH;

#ifdef XBF_DEBUG
	if (sc->sc_caps) {
		printf("%s: features:", sc->sc_dev.dv_xname);
		if (sc->sc_caps & XBF_CAP_BARRIER)
			printf(" BARRIER");
		if (sc->sc_caps & XBF_CAP_FLUSH)
			printf(" FLUSH");
		printf("\n");
	}
#endif

	if (xs_setprop(sc->sc_parent, sc->sc_node, "state",
	    XEN_STATE_CONNECTED, strlen(XEN_STATE_CONNECTED))) {
		printf("%s: failed to set state to CONNECTED\n",
		    sc->sc_dev.dv_xname);
		return (-1);
	}

	sc->sc_state = XBF_CONNECTED;

	return (0);

 errout:
	printf("%s: failed to %s \"%s\" property (%d)\n", sc->sc_dev.dv_xname,
	    action, prop, error);
	xbf_ring_destroy(sc);
	return (-1);
}

int
xbf_dma_alloc(struct xbf_softc *sc, struct xbf_dma_mem *dma,
    bus_size_t size, int nsegs, int mapflags)
{
	int error;

	dma->dma_tag = sc->sc_dmat;

	dma->dma_seg = mallocarray(nsegs, sizeof(bus_dma_segment_t), M_DEVBUF,
	    M_ZERO | M_NOWAIT);
	if (dma->dma_seg == NULL) {
		printf("%s: failed to allocate a segment array\n",
		    sc->sc_dev.dv_xname);
		return (ENOMEM);
	}

	error = bus_dmamap_create(dma->dma_tag, size, nsegs, PAGE_SIZE, 0,
	    BUS_DMA_NOWAIT, &dma->dma_map);
	if (error) {
		printf("%s: failed to create a memory map (%d)\n",
		    sc->sc_dev.dv_xname, error);
		goto errout;
	}

	error = bus_dmamem_alloc(dma->dma_tag, size, PAGE_SIZE, 0,
	    dma->dma_seg, nsegs, &dma->dma_rsegs, BUS_DMA_ZERO |
	    BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: failed to allocate DMA memory (%d)\n",
		    sc->sc_dev.dv_xname, error);
		goto destroy;
	}

	error = bus_dmamem_map(dma->dma_tag, dma->dma_seg, dma->dma_rsegs,
	    size, &dma->dma_vaddr, BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: failed to map DMA memory (%d)\n",
		    sc->sc_dev.dv_xname, error);
		goto free;
	}

	error = bus_dmamap_load(dma->dma_tag, dma->dma_map, dma->dma_vaddr,
	    size, NULL, mapflags | BUS_DMA_NOWAIT);
	if (error) {
		printf("%s: failed to load DMA memory (%d)\n",
		    sc->sc_dev.dv_xname, error);
		goto unmap;
	}

	dma->dma_size = size;
	dma->dma_nsegs = nsegs;
	return (0);

 unmap:
	bus_dmamem_unmap(dma->dma_tag, dma->dma_vaddr, size);
 free:
	bus_dmamem_free(dma->dma_tag, dma->dma_seg, dma->dma_rsegs);
 destroy:
	bus_dmamap_destroy(dma->dma_tag, dma->dma_map);
 errout:
	free(dma->dma_seg, M_DEVBUF, nsegs * sizeof(bus_dma_segment_t));
	dma->dma_map = NULL;
	dma->dma_tag = NULL;
	return (error);
}

void
xbf_dma_free(struct xbf_softc *sc, struct xbf_dma_mem *dma)
{
	if (dma->dma_tag == NULL || dma->dma_map == NULL)
		return;
	bus_dmamap_sync(dma->dma_tag, dma->dma_map, 0, dma->dma_size,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(dma->dma_tag, dma->dma_map);
	bus_dmamem_unmap(dma->dma_tag, dma->dma_vaddr, dma->dma_size);
	bus_dmamem_free(dma->dma_tag, dma->dma_seg, dma->dma_rsegs);
	bus_dmamap_destroy(dma->dma_tag, dma->dma_map);
	free(dma->dma_seg, M_DEVBUF, dma->dma_nsegs * sizeof(bus_dma_segment_t));
	dma->dma_seg = NULL;
	dma->dma_map = NULL;
	dma->dma_size = 0;
}

int
xbf_ring_create(struct xbf_softc *sc)
{
	int i, error, nsegs;

	if (xbf_dma_alloc(sc, &sc->sc_xr_dma, sc->sc_xr_size * PAGE_SIZE,
	    sc->sc_xr_size, sc->sc_domid << 16))
		return (-1);
	for (i = 0; i < sc->sc_xr_dma.dma_map->dm_nsegs; i++)
		sc->sc_xr_ref[i] = sc->sc_xr_dma.dma_map->dm_segs[i].ds_addr;

	sc->sc_xr = (struct xbf_ring *)sc->sc_xr_dma.dma_vaddr;

	sc->sc_xr->xr_prod_event = sc->sc_xr->xr_cons_event = 1;

	/* SCSI transfer map */
	sc->sc_xs_map = mallocarray(sc->sc_xr_ndesc, sizeof(bus_dmamap_t),
	    M_DEVBUF, M_ZERO | M_NOWAIT);
	if (sc->sc_xs_map == NULL) {
		printf("%s: failed to allocate scsi transfer map\n",
		    sc->sc_dev.dv_xname);
		goto errout;
	}
	sc->sc_xs = mallocarray(sc->sc_xr_ndesc, sizeof(struct scsi_xfer *),
	    M_DEVBUF, M_ZERO | M_NOWAIT);
	if (sc->sc_xs == NULL) {
		printf("%s: failed to allocate scsi transfer array\n",
		    sc->sc_dev.dv_xname);
		goto errout;
	}
	sc->sc_xs_avail = sc->sc_xr_ndesc;

	/* Bounce buffer maps for unaligned buffers */
	sc->sc_xs_bb = mallocarray(sc->sc_xr_ndesc, sizeof(struct xbf_dma_mem),
	    M_DEVBUF, M_ZERO | M_NOWAIT);
	if (sc->sc_xs_bb == NULL) {
		printf("%s: failed to allocate bounce buffer maps\n",
		    sc->sc_dev.dv_xname);
		goto errout;
	}

	nsegs = MIN(MAXPHYS / PAGE_SIZE, XBF_MAX_SGE);
	sc->sc_maxphys = nsegs * PAGE_SIZE;

	for (i = 0; i < sc->sc_xr_ndesc; i++) {
		error = bus_dmamap_create(sc->sc_dmat, sc->sc_maxphys, nsegs,
		    PAGE_SIZE, PAGE_SIZE, BUS_DMA_NOWAIT, &sc->sc_xs_map[i]);
		if (error) {
			printf("%s: failed to create a memory map for "
			    "the xfer %d (%d)\n", sc->sc_dev.dv_xname, i,
			    error);
			goto errout;
		}
		sc->sc_xr->xr_desc[i].xrd_req.req_id = i;
	}

	return (0);

 errout:
 	xbf_ring_destroy(sc);
 	return (-1);
}

void
xbf_ring_destroy(struct xbf_softc *sc)
{
	int i;

	for (i = 0; i < sc->sc_xr_ndesc; i++) {
		if (sc->sc_xs_map == NULL)
			break;
		if (sc->sc_xs_map[i] == NULL)
			continue;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_xs_map[i], 0, 0,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, sc->sc_xs_map[i]);
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_xs_map[i]);
		sc->sc_xs_map[i] = NULL;
		if (sc->sc_xs == NULL || sc->sc_xs[i] == NULL)
			continue;
		xbf_scsi_done(sc->sc_xs[i], XS_RESET);
	}

	free(sc->sc_xs, M_DEVBUF, sc->sc_xr_ndesc *
	    sizeof(struct scsi_xfer *));
	sc->sc_xs = NULL;

	free(sc->sc_xs_map, M_DEVBUF, sc->sc_xr_ndesc *
	    sizeof(bus_dmamap_t));
	sc->sc_xs_map = NULL;

	free(sc->sc_xs_bb, M_DEVBUF, sc->sc_xr_ndesc *
	    sizeof(struct xbf_dma_mem));
	sc->sc_xs_bb = NULL;

	xbf_dma_free(sc, &sc->sc_xr_dma);

	sc->sc_xr = NULL;
}

void
xbf_stop(struct xbf_softc *sc)
{
	union xbf_ring_desc *xrd;
	struct scsi_xfer *xs;
	bus_dmamap_t map;
	int desc;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_xr_dma.dma_map, 0,
	    sc->sc_xr_dma.dma_map->dm_mapsize, BUS_DMASYNC_POSTREAD |
	    BUS_DMASYNC_POSTWRITE);

	for (desc = 0; desc < sc->sc_xr_ndesc; desc++) {
		xs = sc->sc_xs[desc];
		if (xs == NULL)
			continue;
		xrd = &sc->sc_xr->xr_desc[desc];
		DPRINTF("%s: aborting desc %u(%llu) op %u\n",
		    sc->sc_dev.dv_xname, desc, xrd->xrd_rsp.rsp_id,
		    xrd->xrd_rsp.rsp_op);
		if (sc->sc_xs_bb[desc].dma_size > 0)
			map = sc->sc_xs_bb[desc].dma_map;
		else
			map = sc->sc_xs_map[desc];
		bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, map);
		xbf_reclaim_xs(xs, desc);
		xbf_scsi_done(xs, XS_SELTIMEOUT);
		sc->sc_xs[desc] = NULL;
	}

	/* Give other processes a chance to run */
	yield();

	xbf_ring_destroy(sc);
}
