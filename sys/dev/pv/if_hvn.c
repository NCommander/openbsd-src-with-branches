/*-
 * Copyright (c) 2009-2012 Microsoft Corp.
 * Copyright (c) 2010-2012 Citrix Inc.
 * Copyright (c) 2012 NetApp Inc.
 * Copyright (c) 2016 Mike Belopuhov <mike@esdenera.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * The OpenBSD port was done under funding by Esdenera Networks GmbH.
 */

#include "bpfilter.h"
#include "vlan.h"
#include "hyperv.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/atomic.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/pool.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/task.h>
#include <sys/timeout.h>

#include <machine/bus.h>

#include <uvm/uvm_extern.h>

#include <dev/pv/hypervreg.h>
#include <dev/pv/hypervvar.h>
#include <dev/pv/rndisreg.h>
#include <dev/pv/if_hvnreg.h>

#include <net/if.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#define HVN_DEBUG			1

/*
 * RNDIS control interface
 */
#define HVN_RNDIS_CTLREQS		4
#define HVN_RNDIS_CMPBUFSZ		512

struct rndis_cmd {
	uint32_t			 rc_id;
	struct rndis			*rc_req;
	bus_dmamap_t			 rc_dmap;
	uint64_t			 rc_pfn;
	struct rndis			 rc_cmp;
	uint32_t			 rc_cmplen;
	uint8_t				 rc_cmpbuf[HVN_RNDIS_CMPBUFSZ];
	struct nvsp			 rc_nvsp;
	struct mutex			 rc_mtx;
	TAILQ_ENTRY(rndis_cmd)		 rc_entry;
};
TAILQ_HEAD(rndis_queue, rndis_cmd);

/*
 * Rx ring
 */
#define HVN_RX_BUFID			0xcafe

/*
 * Tx ring
 */
#define HVN_TX_DESC			128
#define HVN_TX_BUFID			0xface

struct hvn_softc {
	struct device			 sc_dev;
	struct hv_softc			*sc_hvsc;
	struct hv_channel		*sc_chan;
	bus_dma_tag_t			 sc_dmat;

	struct arpcom			 sc_ac;
	struct ifmedia			 sc_media;
	int				 sc_link_state;
	int				 sc_promisc;

	/* NVSP protocol */
	int				 sc_proto;
	uint32_t			 sc_nvsptid;
	struct nvsp			 sc_nvsp;
	uint8_t				*sc_nvspbuf;
#define  HVN_NVSP_BUFSIZE		 (PAGE_SIZE * 4)
	struct mutex			 sc_nvsplck;

	/* RNDIS protocol */
	uint32_t			 sc_rndisrid;
	struct rndis_queue		 sc_cntl_sq; /* submission queue */
	struct mutex			 sc_cntl_sqlck;
	struct rndis_queue		 sc_cntl_cq; /* completion queue */
	struct mutex			 sc_cntl_cqlck;
	struct rndis_queue		 sc_cntl_fq; /* free queue */
	struct mutex			 sc_cntl_fqlck;
	struct rndis_cmd		 sc_cntl_msgs[HVN_RNDIS_CTLREQS];

	/* Rx ring */
	void				*sc_rx_ring;
	int				 sc_rx_size;
	uint32_t			 sc_rx_hndl;

	/* Tx ring */
	void				*sc_tx_ring;
	int				 sc_tx_size;
	uint32_t			 sc_tx_hndl;
};

int	hvn_match(struct device *, void *, void *);
void	hvn_attach(struct device *, struct device *, void *);
int	hvn_ioctl(struct ifnet *, u_long, caddr_t);
int	hvn_media_change(struct ifnet *);
void	hvn_media_status(struct ifnet *, struct ifmediareq *);
int	hvn_iff(struct hvn_softc *);
void	hvn_init(struct hvn_softc *);
void	hvn_stop(struct hvn_softc *);
void	hvn_start(struct ifnet *);
void	hvn_txeof(struct hvn_softc *, uint64_t);
void	hvn_rxeof(struct hvn_softc *, void *);
int	hvn_rx_ring_create(struct hvn_softc *);
int	hvn_rx_ring_destroy(struct hvn_softc *);
int	hvn_tx_ring_create(struct hvn_softc *);
int	hvn_tx_ring_destroy(struct hvn_softc *);
int	hvn_get_lladdr(struct hvn_softc *);
int	hvn_set_lladdr(struct hvn_softc *);
void	hvn_get_link_status(struct hvn_softc *);
void	hvn_link_status(struct hvn_softc *);

/* NSVP */
int	hvn_nvsp_attach(struct hvn_softc *);
void	hvn_nvsp_intr(void *);
int	hvn_nvsp_output(struct hvn_softc *, struct nvsp *, uint64_t, int);
int	hvn_nvsp_ack(struct hvn_softc *, struct nvsp *, uint64_t);
void	hvn_nvsp_detach(struct hvn_softc *);

/* RNDIS */
int	hvn_rndis_attach(struct hvn_softc *);
int	hvn_rndis_ctloutput(struct hvn_softc *, struct rndis_cmd *, int);
void	hvn_rndis_filter(struct hvn_softc *sc, uint64_t, void *);
void	hvn_rndis_input(struct hvn_softc *, caddr_t, uint32_t,
	    struct mbuf_list *);
void	hvn_rndis_complete(struct hvn_softc *, caddr_t, uint32_t);
void	hvn_rndis_status(struct hvn_softc *, caddr_t, uint32_t);
int	hvn_rndis_query(struct hvn_softc *, uint32_t, void *, size_t *);
int	hvn_rndis_set(struct hvn_softc *, uint32_t, void *, size_t);
int	hvn_rndis_open(struct hvn_softc *);
int	hvn_rndis_close(struct hvn_softc *);
void	hvn_rndis_detach(struct hvn_softc *);

struct cfdriver hvn_cd = {
	NULL, "hvn", DV_IFNET
};

const struct cfattach hvn_ca = {
	sizeof(struct hvn_softc), hvn_match, hvn_attach
};

int
hvn_match(struct device *parent, void *match, void *aux)
{
	struct hv_attach_args *aa = aux;

	if (strcmp("network", aa->aa_ident))
		return (0);

	return (1);
}

void
hvn_attach(struct device *parent, struct device *self, void *aux)
{
	struct hv_attach_args *aa = aux;
	struct hvn_softc *sc = (struct hvn_softc *)self;
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	sc->sc_hvsc = (struct hv_softc *)parent;
	sc->sc_chan = aa->aa_chan;
	sc->sc_dmat = aa->aa_dmat;

	strlcpy(ifp->if_xname, sc->sc_dev.dv_xname, IFNAMSIZ);

	DPRINTF("\n");

	if (hvn_nvsp_attach(sc)) {
		printf(": failed to init NVSP\n");
		return;
	}

	if (hvn_rx_ring_create(sc)) {
		printf(": failed to create Rx ring\n");
		goto detach;
	}

	if (hvn_tx_ring_create(sc)) {
		printf(": failed to create Tx ring\n");
		goto detach;
	}

	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_xflags = IFXF_MPSAFE;
	ifp->if_ioctl = hvn_ioctl;
	ifp->if_start = hvn_start;
	ifp->if_softc = sc;

	ifp->if_capabilities = IFCAP_CSUM_IPv4 | IFCAP_CSUM_TCPv4 |
	    IFCAP_CSUM_UDPv4;
#if NVLAN > 0
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
#endif

	IFQ_SET_MAXLEN(&ifp->if_snd, HVN_TX_DESC - 1);

	ifmedia_init(&sc->sc_media, IFM_IMASK, hvn_media_change,
	    hvn_media_status);
	ifmedia_add(&sc->sc_media, IFM_ETHER | IFM_MANUAL, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER | IFM_MANUAL);

	if_attach(ifp);

	if (hvn_rndis_attach(sc)) {
		printf(": failed to init RNDIS\n");
		goto detach;
	}

	if (hvn_get_lladdr(sc)) {
		printf(": failed to obtain an ethernet address\n");
		hvn_rndis_detach(sc);
		goto detach;
	}

	DPRINTF("%s", sc->sc_dev.dv_xname);
	printf(": channel %u, address %s\n", sc->sc_chan->ch_id,
	    ether_sprintf(sc->sc_ac.ac_enaddr));

	ether_ifattach(ifp);
	return;

 detach:
	hvn_rx_ring_destroy(sc);
	hvn_tx_ring_destroy(sc);
	hvn_nvsp_detach(sc);
	if (ifp->if_start)
		if_detach(ifp);
}

int
hvn_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct hvn_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();

	switch (command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		if (!(ifp->if_flags & IFF_RUNNING))
			hvn_init(sc);
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				error = ENETRESET;
			else
				hvn_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				hvn_stop(sc);
		}
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, command);
		break;
	default:
		error = ether_ioctl(ifp, &sc->sc_ac, command, data);
	}

	if (error == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING)
			hvn_iff(sc);
		error = 0;
	}

	splx(s);

	return (error);
}

int
hvn_media_change(struct ifnet *ifp)
{
	return (0);
}

void
hvn_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct hvn_softc *sc = ifp->if_softc;

	hvn_get_link_status(sc);
	hvn_link_status(sc);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER | IFM_MANUAL;
	if (sc->sc_link_state == LINK_STATE_UP)
		ifmr->ifm_status |= IFM_ACTIVE;
}

void
hvn_link_status(struct hvn_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	if (sc->sc_link_state != ifp->if_link_state) {
		ifp->if_link_state = sc->sc_link_state;
		if_link_state_change(ifp);
	}
}

int
hvn_iff(struct hvn_softc *sc)
{
	/* XXX */
	sc->sc_promisc = 0;

	return (0);
}

void
hvn_init(struct hvn_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	hvn_stop(sc);

	hvn_iff(sc);

	if (hvn_rndis_open(sc) == 0) {
		ifp->if_flags |= IFF_RUNNING;
		ifq_clr_oactive(&ifp->if_snd);
	}
}

void
hvn_stop(struct hvn_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	if (ifp->if_flags & IFF_RUNNING) {
		ifp->if_flags &= ~IFF_RUNNING;
		hvn_rndis_close(sc);
	}

	ifq_barrier(&ifp->if_snd);
	intr_barrier(sc->sc_chan);

	ifq_clr_oactive(&ifp->if_snd);
}

void
hvn_start(struct ifnet *ifp)
{
	struct mbuf *m;

	if (!(ifp->if_flags & IFF_RUNNING) || ifq_is_oactive(&ifp->if_snd))
		return;

	for (;;) {
		m = ifq_deq_begin(&ifp->if_snd);
		if (m == NULL)
			break;

		ifq_deq_commit(&ifp->if_snd, m);

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap_ether(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif

		m_freem(m);
		ifp->if_oerrors++;
	}
}

void
hvn_txeof(struct hvn_softc *sc, uint64_t tid)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	printf("%s: tx tid %llu\n", ifp->if_xname, tid);
}

int
hvn_rx_ring_create(struct hvn_softc *sc)
{
	struct nvsp *pkt = &sc->sc_nvsp;
	struct nvsp_send_rx_buf *msg;
	struct nvsp_send_rx_buf_comp *cmp;
	uint64_t tid;

	if (sc->sc_proto <= NVSP_PROTOCOL_VERSION_2)
		sc->sc_rx_size = 15 * 1024 * 1024;	/* 15MB */
	else
		sc->sc_rx_size = 16 * 1024 * 1024; 	/* 16MB */
	sc->sc_rx_ring = km_alloc(sc->sc_rx_size, &kv_any, &kp_zero,
	    cold ? &kd_nowait : &kd_waitok);
	if (sc->sc_rx_ring == NULL) {
		DPRINTF("%s: failed to allocate Rx ring buffer\n",
		    sc->sc_dev.dv_xname);
		return (-1);
	}
	if (hv_handle_alloc(sc->sc_chan, sc->sc_rx_ring, sc->sc_rx_size,
	    &sc->sc_rx_hndl)) {
		DPRINTF("%s: failed to obtain a PA handle\n",
		    sc->sc_dev.dv_xname);
		goto errout;
	}

	memset(pkt, 0, sizeof(*pkt));
	pkt->msg_type = nvsp_type_send_rx_buf;
	msg = (struct nvsp_send_rx_buf *)&pkt->msg;
	msg->gpadl_handle = sc->sc_rx_hndl;
	msg->id = HVN_RX_BUFID;

	tid = atomic_inc_int_nv(&sc->sc_nvsptid);
	if (hvn_nvsp_output(sc, pkt, tid, 100))
		goto errout;

	cmp = (struct nvsp_send_rx_buf_comp *)&pkt->msg;
	if (cmp->status != nvsp_status_success) {
		DPRINTF("%s: failed to set up the Rx ring\n",
		    sc->sc_dev.dv_xname);
		goto errout;
	}
	if (cmp->num_sections > 1) {
		DPRINTF("%s: invalid number of Rx ring sections: %d\n",
		    sc->sc_dev.dv_xname, cmp->num_sections);
		hvn_rx_ring_destroy(sc);
		return (-1);
	}
	return (0);

 errout:
	if (sc->sc_rx_hndl) {
		hv_handle_free(sc->sc_chan, sc->sc_rx_hndl);
		sc->sc_rx_hndl = 0;
	}
	if (sc->sc_rx_ring) {
		km_free(sc->sc_rx_ring, sc->sc_rx_size, &kv_any, &kp_zero);
		sc->sc_rx_ring = NULL;
	}
	return (-1);
}

int
hvn_rx_ring_destroy(struct hvn_softc *sc)
{
	struct nvsp *pkt = &sc->sc_nvsp;
	struct nvsp_revoke_rx_buf *msg;
	uint64_t tid;

	if (sc->sc_rx_ring == NULL)
		return (0);

	memset(pkt, 0, sizeof(*pkt));
	pkt->msg_type = nvsp_type_revoke_rx_buf;
	msg = (struct nvsp_revoke_rx_buf *)&pkt->msg;
	msg->id = HVN_RX_BUFID;

	tid = atomic_inc_int_nv(&sc->sc_nvsptid);
	if (hvn_nvsp_output(sc, pkt, tid, 0))
		return (-1);

	delay(100);

	hv_handle_free(sc->sc_chan, sc->sc_rx_hndl);

	sc->sc_rx_hndl = 0;

	km_free(sc->sc_rx_ring, sc->sc_rx_size, &kv_any, &kp_zero);
	sc->sc_rx_ring = NULL;

	return (0);
}

int
hvn_tx_ring_create(struct hvn_softc *sc)
{
	struct nvsp *pkt = &sc->sc_nvsp;
	struct nvsp_send_tx_buf *msg;
	struct nvsp_send_tx_buf_comp *cmp;
	uint64_t tid;

	sc->sc_tx_size = 15 * 1024 * 1024;	/* 15MB */
	sc->sc_tx_ring = km_alloc(sc->sc_tx_size, &kv_any, &kp_zero,
	    cold ? &kd_nowait : &kd_waitok);
	if (sc->sc_tx_ring == NULL) {
		DPRINTF("%s: failed to allocate Tx ring buffer\n",
		    sc->sc_dev.dv_xname);
		return (-1);
	}
	if (hv_handle_alloc(sc->sc_chan, sc->sc_tx_ring, sc->sc_tx_size,
	    &sc->sc_tx_hndl)) {
		DPRINTF("%s: failed to obtain a PA handle\n",
		    sc->sc_dev.dv_xname);
		goto errout;
	}

	memset(pkt, 0, sizeof(*pkt));
	pkt->msg_type = nvsp_type_send_tx_buf;
	msg = (struct nvsp_send_tx_buf *)&pkt->msg;
	msg->gpadl_handle = sc->sc_tx_hndl;
	msg->id = HVN_TX_BUFID;

	tid = atomic_inc_int_nv(&sc->sc_nvsptid);
	if (hvn_nvsp_output(sc, pkt, tid, 100))
		goto errout;

	cmp = (struct nvsp_send_tx_buf_comp *)&pkt->msg;
	if (cmp->status != nvsp_status_success) {
		DPRINTF("%s: failed to set up the Tx ring\n",
		    sc->sc_dev.dv_xname);
		goto errout;
	}
	return (0);

 errout:
	if (sc->sc_tx_hndl) {
		hv_handle_free(sc->sc_chan, sc->sc_tx_hndl);
		sc->sc_tx_hndl = 0;
	}
	if (sc->sc_tx_ring) {
		km_free(sc->sc_tx_ring, sc->sc_tx_size, &kv_any, &kp_zero);
		sc->sc_tx_ring = NULL;
	}
	return (-1);
}

int
hvn_tx_ring_destroy(struct hvn_softc *sc)
{
	struct nvsp *pkt = &sc->sc_nvsp;
	struct nvsp_revoke_tx_buf *msg;
	uint64_t tid;

	if (sc->sc_tx_ring == NULL)
		return (0);

	memset(pkt, 0, sizeof(*pkt));
	pkt->msg_type = nvsp_type_revoke_tx_buf;
	msg = (struct nvsp_revoke_tx_buf *)&pkt->msg;
	msg->id = HVN_TX_BUFID;

	tid = atomic_inc_int_nv(&sc->sc_nvsptid);
	if (hvn_nvsp_output(sc, pkt, tid, 0))
		return (-1);

	delay(100);

	hv_handle_free(sc->sc_chan, sc->sc_tx_hndl);

	sc->sc_tx_hndl = 0;

	km_free(sc->sc_tx_ring, sc->sc_tx_size, &kv_any, &kp_zero);
	sc->sc_tx_ring = NULL;

	return (0);
}

int
hvn_get_lladdr(struct hvn_softc *sc)
{
	char enaddr[ETHER_ADDR_LEN];
	size_t addrlen = ETHER_ADDR_LEN;
	int rv;

	rv = hvn_rndis_query(sc, RNDIS_OID_802_3_PERMANENT_ADDRESS,
	    enaddr, &addrlen);
	if (rv == 0 && addrlen == ETHER_ADDR_LEN)
		memcpy(sc->sc_ac.ac_enaddr, enaddr, ETHER_ADDR_LEN);
	return (rv);
}

int
hvn_set_lladdr(struct hvn_softc *sc)
{
	return (hvn_rndis_set(sc, RNDIS_OID_802_3_CURRENT_ADDRESS,
	    sc->sc_ac.ac_enaddr, ETHER_ADDR_LEN));
}

void
hvn_get_link_status(struct hvn_softc *sc)
{
	uint32_t state;
	size_t len = sizeof(state);

	if (hvn_rndis_query(sc, RNDIS_OID_GEN_MEDIA_CONNECT_STATUS,
	    &state, &len) == 0)
		sc->sc_link_state = (state == 0) ? LINK_STATE_UP :
		    LINK_STATE_DOWN;
}

int
hvn_nvsp_attach(struct hvn_softc *sc)
{
	const uint32_t protos[] = {
		NVSP_PROTOCOL_VERSION_5, NVSP_PROTOCOL_VERSION_4,
		NVSP_PROTOCOL_VERSION_2, NVSP_PROTOCOL_VERSION_1
	};
	struct nvsp *pkt = &sc->sc_nvsp;
	struct nvsp_init *init;
	struct nvsp_init_comp *cmp;
	struct nvsp_send_ndis_version *ver;
	uint64_t tid;
	uint32_t ndisver;
	int i;

	/* 4 page sized buffer for channel messages */
	sc->sc_nvspbuf = km_alloc(HVN_NVSP_BUFSIZE, &kv_any, &kp_zero,
	    (cold ? &kd_nowait : &kd_waitok));
	if (sc->sc_nvspbuf == NULL) {
		DPRINTF("%s: failed to allocate channel data buffer\n",
		    sc->sc_dev.dv_xname);
		return (-1);
	}
	sc->sc_chan->ch_buflen = PAGE_SIZE * 4;

	/* Associate our interrupt handler with the channel */
	if (hv_channel_open(sc->sc_chan, NULL, 0, hvn_nvsp_intr, sc)) {
		DPRINTF("%s: failed to open channel\n", sc->sc_dev.dv_xname);
		km_free(sc->sc_nvspbuf, HVN_NVSP_BUFSIZE, &kv_any, &kp_zero);
		return (-1);
	}

	mtx_init(&sc->sc_nvsplck, IPL_NET);

	memset(pkt, 0, sizeof(*pkt));
	pkt->msg_type = nvsp_type_init;
	init = (struct nvsp_init *)&pkt->msg;

	for (i = 0; i < nitems(protos); i++) {
		init->protocol_version = init->protocol_version_2 = protos[i];
		tid = atomic_inc_int_nv(&sc->sc_nvsptid);
		if (hvn_nvsp_output(sc, pkt, tid, 100))
			return (-1);
		cmp = (struct nvsp_init_comp *)&pkt->msg;
		if (cmp->status == nvsp_status_success) {
			sc->sc_proto = protos[i];
			break;
		}
	}
	if (!sc->sc_proto) {
		DPRINTF("%s: failed to negotiate NVSP version\n",
		    sc->sc_dev.dv_xname);
		return (-1);
	}

	memset(pkt, 0, sizeof(*pkt));
	pkt->msg_type = nvsp_type_send_ndis_vers;
	ver = (struct nvsp_send_ndis_version *)&pkt->msg;
	if (sc->sc_proto <= NVSP_PROTOCOL_VERSION_4)
		ndisver = NDIS_VERSION_6_1;
	else
		ndisver = NDIS_VERSION_6_30;
	ver->ndis_major_vers = (ndisver & 0xffff0000) >> 16;
	ver->ndis_minor_vers = (ndisver & 0x0000ffff);

	tid = atomic_inc_int_nv(&sc->sc_nvsptid);
	if (hvn_nvsp_output(sc, pkt, tid, 100))
		return (-1);

	DPRINTF("%s: NVSP %u.%u, NDIS %u.%u\n", sc->sc_dev.dv_xname,
	    sc->sc_proto >> 16, sc->sc_proto & 0xffff,
	    ndisver >> 16, ndisver & 0xffff);

	return (0);
}

void
hvn_nvsp_intr(void *arg)
{
	struct hvn_softc *sc = arg;
	struct vmbus_chanpkt_hdr *cph;
	struct nvsp *pkt;
	uint64_t rid;
	uint32_t rlen;
	int rv;

	for (;;) {
		rv = hv_channel_recv(sc->sc_chan, sc->sc_nvspbuf,
		    HVN_NVSP_BUFSIZE, &rlen, &rid, 1);
		if (rv != 0 || rlen == 0) {
			if (rv != EAGAIN)
				printf("%s: failed to receive an NVSP "
				    "packet\n", sc->sc_dev.dv_xname);
			break;
		}
		cph = (struct vmbus_chanpkt_hdr *)sc->sc_nvspbuf;
		pkt = (struct nvsp *)VMBUS_CHANPKT_CONST_DATA(cph);

		if (cph->cph_type == VMBUS_CHANPKT_TYPE_COMP) {
			switch (pkt->msg_type) {
			case nvsp_type_init_comp:
			case nvsp_type_send_rx_buf_comp:
			case nvsp_type_send_tx_buf_comp:
			case nvsp_type_subchannel:
				/* copy the response back */
				memcpy(&sc->sc_nvsp, pkt, sizeof(sc->sc_nvsp));
				wakeup_one(&sc->sc_nvsp);
				break;
			case nvsp_type_send_rndis_pkt_comp:
				hvn_txeof(sc, cph->cph_tid);
				break;
			default:
				printf("%s: unhandled NVSP packet type %d "
				    "on completion\n", sc->sc_dev.dv_xname,
				    pkt->msg_type);
			}
		} else if (cph->cph_type == VMBUS_CHANPKT_TYPE_RXBUF) {
			switch (pkt->msg_type) {
			case nvsp_type_send_rndis_pkt:
				hvn_rndis_filter(sc, cph->cph_tid, cph);
				break;
			default:
				printf("%s: unhandled NVSP packet type %d "
				    "on receive\n", sc->sc_dev.dv_xname,
				    pkt->msg_type);
			}
		} else
			printf("%s: unknown NVSP packet type %u\n",
			    sc->sc_dev.dv_xname, cph->cph_type);
	}
}

int
hvn_nvsp_output(struct hvn_softc *sc, struct nvsp *pkt, uint64_t tid, int timo)
{
	int tries = 10;
	int rv;

	do {
		rv = hv_channel_send(sc->sc_chan, pkt, sizeof(*pkt),
		    tid, VMBUS_CHANPKT_TYPE_INBAND,
		    timo ? VMBUS_CHANPKT_FLAG_RC : 0);
		if (rv == EAGAIN) {
			if (timo)
				tsleep(pkt, PRIBIO, "hvnsend", timo / 10);
			else
				delay(100);
		} else if (rv) {
			DPRINTF("%s: NVSP operation %d send error %d\n",
			    sc->sc_dev.dv_xname, pkt->msg_type, rv);
			return (rv);
		}
	} while (rv != 0 && --tries > 0);

	if (timo) {
		mtx_enter(&sc->sc_nvsplck);
		rv = msleep(&sc->sc_nvsp, &sc->sc_nvsplck, PRIBIO, "hvnvsp",
		    timo);
		mtx_leave(&sc->sc_nvsplck);
#ifdef HVN_DEBUG
		switch (rv) {
		case EINTR:
			rv = 0;
			break;
		case EWOULDBLOCK:
			printf("%s: NVSP opertaion %d timed out\n",
			    sc->sc_dev.dv_xname, pkt->msg_type);
		}
	}
#endif
	return (rv);
}

int
hvn_nvsp_ack(struct hvn_softc *sc, struct nvsp *pkt, uint64_t tid)
{
	int tries = 5;
	int rv;

	do {
		rv = hv_channel_send(sc->sc_chan, pkt, sizeof(*pkt),
		    tid, VMBUS_CHANPKT_TYPE_COMP, 0);
		if (rv == EAGAIN)
			delay(100);
		else if (rv) {
			DPRINTF("%s: NVSP acknowledgement error %d\n",
			    sc->sc_dev.dv_xname, rv);
			return (rv);
		}
	} while (rv != 0 && --tries > 0);
	return (rv);
}

void
hvn_nvsp_detach(struct hvn_softc *sc)
{
	if (hv_channel_close(sc->sc_chan) == 0) {
		km_free(sc->sc_nvspbuf, HVN_NVSP_BUFSIZE, &kv_any, &kp_zero);
		sc->sc_nvspbuf = NULL;
	}
}

static inline struct rndis_cmd *
hvn_alloc_cmd(struct hvn_softc *sc)
{
	struct rndis_cmd *rc;

	mtx_enter(&sc->sc_cntl_fqlck);
	while ((rc = TAILQ_FIRST(&sc->sc_cntl_fq)) == NULL)
		msleep(&sc->sc_cntl_fq, &sc->sc_cntl_fqlck,
		    PRIBIO, "hvnrr", 1);
	TAILQ_REMOVE(&sc->sc_cntl_fq, rc, rc_entry);
	mtx_leave(&sc->sc_cntl_fqlck);
	return (rc);
}

static inline void
hvn_submit_cmd(struct hvn_softc *sc, struct rndis_cmd *rc)
{
	mtx_enter(&sc->sc_cntl_sqlck);
	TAILQ_INSERT_TAIL(&sc->sc_cntl_sq, rc, rc_entry);
	mtx_leave(&sc->sc_cntl_sqlck);
}

static inline struct rndis_cmd *
hvn_complete_cmd(struct hvn_softc *sc, uint32_t id)
{
	struct rndis_cmd *rc;

	mtx_enter(&sc->sc_cntl_sqlck);
	TAILQ_FOREACH(rc, &sc->sc_cntl_sq, rc_entry) {
		if (rc->rc_id == id) {
			TAILQ_REMOVE(&sc->sc_cntl_sq, rc, rc_entry);
			break;
		}
	}
	mtx_leave(&sc->sc_cntl_sqlck);
	if (rc != NULL) {
		mtx_enter(&sc->sc_cntl_cqlck);
		TAILQ_INSERT_TAIL(&sc->sc_cntl_cq, rc, rc_entry);
		mtx_leave(&sc->sc_cntl_cqlck);
	}
	return (rc);
}

static inline int
hvn_rollback_cmd(struct hvn_softc *sc, struct rndis_cmd *rc)
{
	struct rndis_cmd *rn;

	mtx_enter(&sc->sc_cntl_sqlck);
	TAILQ_FOREACH(rn, &sc->sc_cntl_sq, rc_entry) {
		if (rn == rc) {
			TAILQ_REMOVE(&sc->sc_cntl_sq, rc, rc_entry);
			mtx_leave(&sc->sc_cntl_sqlck);
			return (0);
		}
	}
	mtx_leave(&sc->sc_cntl_sqlck);
	return (-1);
}

static inline void
hvn_free_cmd(struct hvn_softc *sc, struct rndis_cmd *rc)
{
	memset(rc->rc_req, 0, sizeof(*rc->rc_req));
	memset(&rc->rc_cmp, 0, sizeof(rc->rc_cmp));
	memset(&rc->rc_nvsp, 0, sizeof(rc->rc_nvsp));
	mtx_enter(&sc->sc_cntl_fqlck);
	TAILQ_INSERT_TAIL(&sc->sc_cntl_fq, rc, rc_entry);
	mtx_leave(&sc->sc_cntl_fqlck);
	wakeup(&sc->sc_cntl_fq);
}

int
hvn_rndis_attach(struct hvn_softc *sc)
{
	struct rndis_init_req *req;
	struct rndis_init_comp *cmp;
	struct rndis_cmd *rc;
	int i, rv;

	/* RNDIS control message queues */
	TAILQ_INIT(&sc->sc_cntl_sq);
	TAILQ_INIT(&sc->sc_cntl_cq);
	TAILQ_INIT(&sc->sc_cntl_fq);
	mtx_init(&sc->sc_cntl_sqlck, IPL_NET);
	mtx_init(&sc->sc_cntl_cqlck, IPL_NET);
	mtx_init(&sc->sc_cntl_fqlck, IPL_NET);

	for (i = 0; i < HVN_RNDIS_CTLREQS; i++) {
		rc = &sc->sc_cntl_msgs[i];
		if (bus_dmamap_create(sc->sc_dmat, PAGE_SIZE, 1,
		    PAGE_SIZE, 0, BUS_DMA_WAITOK, &rc->rc_dmap)) {
			DPRINTF("%s: failed to create RNDIS command map\n",
			    sc->sc_dev.dv_xname);
			goto errout;
		}
		rc->rc_req = km_alloc(PAGE_SIZE, &kv_any, &kp_zero,
		    &kd_waitok);
		if (rc->rc_req == NULL) {
			DPRINTF("%s: failed to allocate RNDIS command\n",
			    sc->sc_dev.dv_xname);
			bus_dmamap_destroy(sc->sc_dmat, rc->rc_dmap);
			goto errout;
		}
		if (bus_dmamap_load(sc->sc_dmat, rc->rc_dmap, rc->rc_req,
		    PAGE_SIZE, NULL, BUS_DMA_WAITOK)) {
			DPRINTF("%s: failed to load RNDIS command map\n",
			    sc->sc_dev.dv_xname);
			km_free(rc->rc_req, PAGE_SIZE, &kv_any, &kp_zero);
			bus_dmamap_destroy(sc->sc_dmat, rc->rc_dmap);
			goto errout;
		}
		rc->rc_pfn = atop(rc->rc_dmap->dm_segs[0].ds_addr);
		mtx_init(&rc->rc_mtx, IPL_NET);
		TAILQ_INSERT_TAIL(&sc->sc_cntl_fq, rc, rc_entry);
	}

	rc = hvn_alloc_cmd(sc);

	bus_dmamap_sync(sc->sc_dmat, rc->rc_dmap, 0, PAGE_SIZE,
	    BUS_DMASYNC_PREREAD);

	rc->rc_req->msg_type = RNDIS_INITIALIZE_MSG;
	rc->rc_req->msg_len = RNDIS_MESSAGE_SIZE(*req);
	rc->rc_cmplen = RNDIS_MESSAGE_SIZE(*cmp);
	rc->rc_id = atomic_inc_int_nv(&sc->sc_rndisrid);
	req = (struct rndis_init_req *)&rc->rc_req->msg;
	req->request_id = rc->rc_id;
	req->major_version = RNDIS_MAJOR_VERSION;
	req->minor_version = RNDIS_MINOR_VERSION;
	req->max_xfer_size = 2048; /* XXX */

	bus_dmamap_sync(sc->sc_dmat, rc->rc_dmap, 0, PAGE_SIZE,
	    BUS_DMASYNC_PREWRITE);

	if ((rv = hvn_rndis_ctloutput(sc, rc, 500)) != 0) {
		DPRINTF("%s: INITIALIZE_MSG failed, error %u\n",
		    sc->sc_dev.dv_xname, rv);
		hvn_free_cmd(sc, rc);
		goto errout;
	}
	cmp = (struct rndis_init_comp *)&rc->rc_cmp.msg;
	if (cmp->status != RNDIS_STATUS_SUCCESS) {
		DPRINTF("%s: failed to init RNDIS, error %#x\n",
		    sc->sc_dev.dv_xname, cmp->status);
		hvn_free_cmd(sc, rc);
		goto errout;
	}
	DPRINTF("%s: RNDIS %u.%u\n", sc->sc_dev.dv_xname,
	    cmp->major_version, cmp->minor_version);

	hvn_free_cmd(sc, rc);

	return (0);

errout:
	for (i = 0; i < HVN_RNDIS_CTLREQS; i++) {
		rc = &sc->sc_cntl_msgs[i];
		if (rc->rc_req == NULL)
			continue;
		TAILQ_REMOVE(&sc->sc_cntl_fq, rc, rc_entry);
		km_free(rc->rc_req, PAGE_SIZE, &kv_any, &kp_zero);
		rc->rc_req = NULL;
		bus_dmamap_destroy(sc->sc_dmat, rc->rc_dmap);
	}
	return (-1);
}

int
hvn_rndis_ctloutput(struct hvn_softc *sc, struct rndis_cmd *rc, int timo)
{
	struct nvsp_send_rndis_pkt *msg;
	struct vmbus_gpa sgl[1];
	int tries = 10;
	int rv;

	KASSERT(timo > 0);

	rc->rc_nvsp.msg_type = nvsp_type_send_rndis_pkt;
	msg = (struct nvsp_send_rndis_pkt *)&rc->rc_nvsp.msg;
	msg->chan_type = 1; /* control */
	msg->send_buf_section_idx = NVSP_INVALID_SECTION_INDEX;

	sgl[0].gpa_page = rc->rc_pfn;
	sgl[0].gpa_len = rc->rc_req->msg_len;
	sgl[0].gpa_ofs = 0;

	hvn_submit_cmd(sc, rc);

	do {
		rv = hv_channel_send_sgl(sc->sc_chan, sgl, 1, &rc->rc_nvsp,
		    sizeof(struct nvsp), rc->rc_id);
		if (rv == EAGAIN)
			tsleep(rc, PRIBIO, "hvnsendbuf", timo / 10);
		else if (rv) {
			DPRINTF("%s: RNDIS operation %d send error %d\n",
			    sc->sc_dev.dv_xname, rc->rc_req->msg_type, rv);
			return (rv);
		}
	} while (rv != 0 && --tries > 0);

	bus_dmamap_sync(sc->sc_dmat, rc->rc_dmap, 0, PAGE_SIZE,
	    BUS_DMASYNC_POSTWRITE);

	mtx_enter(&rc->rc_mtx);
	rv = msleep(rc, &rc->rc_mtx, PRIBIO, "rndisctl", timo);
	mtx_leave(&rc->rc_mtx);

	bus_dmamap_sync(sc->sc_dmat, rc->rc_dmap, 0, PAGE_SIZE,
	    BUS_DMASYNC_POSTREAD);

#ifdef HVN_DEBUG
	switch (rv) {
	case EINTR:
		rv = 0;
		break;
	case EWOULDBLOCK:
		if (hvn_rollback_cmd(sc, rc)) {
			/* failed to rollback? go for one sleep cycle */
			tsleep(rc, PRIBIO, "rndisctl2", 1);
			rv = 0;
			break;
		}
		printf("%s: RNDIS opertaion %d timed out\n", sc->sc_dev.dv_xname,
		    rc->rc_req->msg_type);
	}
#endif
	return (rv);
}

void
hvn_rndis_filter(struct hvn_softc *sc, uint64_t tid, void *arg)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	struct nvsp pkt;
	struct vmbus_chanpkt_prplist *cp = arg;
	struct nvsp_send_rndis_pkt_comp *cmp;
	uint32_t off, len, type, status = 0;
	int i;

	if (sc->sc_rx_ring == NULL) {
		DPRINTF("%s: invalid rx ring\n", sc->sc_dev.dv_xname);
		return;
	}
	for (i = 0; i < cp->cp_range_cnt; i++) {
		off = cp->cp_range[i].gpa_ofs;
		len = cp->cp_range[i].gpa_len;

		KASSERT(off + len <= sc->sc_rx_size);
		KASSERT(len >= RNDIS_HEADER_SIZE + 4);

		memcpy(&type, (caddr_t)sc->sc_rx_ring + off, sizeof(type));
		switch (type) {
		/* data message */
		case RNDIS_PACKET_MSG:
			hvn_rndis_input(sc, (caddr_t)sc->sc_rx_ring +
			    off, len, &ml);
			break;
		/* completion messages */
		case RNDIS_INITIALIZE_CMPLT:
		case RNDIS_QUERY_CMPLT:
		case RNDIS_SET_CMPLT:
		case RNDIS_RESET_CMPLT:
		case RNDIS_KEEPALIVE_CMPLT:
			hvn_rndis_complete(sc, (caddr_t)sc->sc_rx_ring +
			    off, len);
			break;
		/* notification message */
		case RNDIS_INDICATE_STATUS_MSG:
			hvn_rndis_status(sc, (caddr_t)sc->sc_rx_ring +
			    off, len);
			break;
		default:
			printf("%s: unhandled RNDIS message type %u\n",
			    sc->sc_dev.dv_xname, type);
		}
	}
	memset(&pkt, 0, sizeof(pkt));
	pkt.msg_type = nvsp_type_send_rndis_pkt_comp;
	cmp = (struct nvsp_send_rndis_pkt_comp *)&pkt.msg;
	cmp->status = status;	/* XXX */
	hvn_nvsp_ack(sc, &pkt, tid);

	if (MBUF_LIST_FIRST(&ml))
		if_input(ifp, &ml);
}

static inline struct mbuf *
hvn_devget(struct hvn_softc *sc, caddr_t buf, uint32_t len)
{
	struct mbuf *m;

	if (len + ETHER_ALIGN <= MHLEN)
		MGETHDR(m, M_NOWAIT, MT_DATA);
	else
		m = MCLGETI(NULL, M_NOWAIT, NULL, len + ETHER_ALIGN);
	if (m == NULL)
		return (NULL);
	m->m_len = m->m_pkthdr.len = len;
	m_adj(m, ETHER_ALIGN);

	if (m_copyback(m, 0, len, buf, M_NOWAIT)) {
		m_freem(m);
		return (NULL);
	}

	return (m);
}

void
hvn_rndis_input(struct hvn_softc *sc, caddr_t buf, uint32_t len,
    struct mbuf_list *ml)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct rndis_pkt *pkt;
	struct rndis_pkt_info *ppi;
	struct rndis_tcp_ip_csum_info *csum;
	struct ndis_8021q_info *vlan;
	struct mbuf *m;

	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	if (len < RNDIS_HEADER_SIZE + sizeof(*pkt)) {
		printf("%s: data packet too short: %u\n",
		    sc->sc_dev.dv_xname, len);
		return;
	}

	pkt = (struct rndis_pkt *)(buf + RNDIS_HEADER_SIZE);

	if (pkt->data_offset + pkt->data_length > len) {
		printf("%s: data packet out of bounds: %u@%u\n",
		    sc->sc_dev.dv_xname, pkt->data_offset, pkt->data_length);
		return;
	}

	if ((m = hvn_devget(sc, buf + RNDIS_HEADER_SIZE + pkt->data_offset,
	    pkt->data_length)) == NULL) {
		ifp->if_ierrors++;
		return;
	}

	while (pkt->pkt_info_length > 0) {
		if (pkt->pkt_info_offset + pkt->pkt_info_length > len) {
			printf("%s: PPI out of bounds: %u@%u\n",
			    sc->sc_dev.dv_xname, pkt->pkt_info_length,
			    pkt->pkt_info_offset);
			break;
		}
		ppi = (struct rndis_pkt_info *)((caddr_t)pkt +
		    pkt->pkt_info_offset);
		if (ppi->size > pkt->pkt_info_length) {
			printf("%s: invalid PPI size: %u/%u\n",
			    sc->sc_dev.dv_xname, ppi->size,
			    pkt->pkt_info_length);
			break;
		}
		switch (ppi->type) {
		case tcpip_chksum_info:
			csum = (struct rndis_tcp_ip_csum_info *)
			    ((caddr_t)ppi + ppi->size);
			if (csum->recv.ip_csum_succeeded)
				m->m_pkthdr.csum_flags |= M_IPV4_CSUM_IN_OK;
			if (csum->recv.tcp_csum_succeeded)
				m->m_pkthdr.csum_flags |= M_TCP_CSUM_IN_OK;
			if (csum->recv.udp_csum_succeeded)
				m->m_pkthdr.csum_flags |= M_UDP_CSUM_IN_OK;
			break;
		case ieee_8021q_info:
			vlan = (struct ndis_8021q_info *)
			    ((caddr_t)ppi + ppi->size);
#if NVLAN > 0
			m->m_pkthdr.ether_vtag = vlan->vlan_id;
			m->m_flags |= M_VLANTAG;
#endif
			break;
		default:
			DPRINTF("%s: unhandled PPI %u\n", sc->sc_dev.dv_xname,
			    ppi->type);
		}
		pkt->pkt_info_length -= ppi->size;
	}

	ml_enqueue(ml, m);
}

void
hvn_rndis_complete(struct hvn_softc *sc, caddr_t buf, uint32_t len)
{
	struct rndis_cmd *rc;
	uint32_t id;

	memcpy(&id, buf + RNDIS_HEADER_SIZE, sizeof(id));
	if ((rc = hvn_complete_cmd(sc, id)) != NULL) {
		if (len < rc->rc_cmplen)
			printf("%s: RNDIS response %u too short: %u\n",
			    sc->sc_dev.dv_xname, id, len);
		else
			memcpy(&rc->rc_cmp, buf, rc->rc_cmplen);
		if (len > rc->rc_cmplen &&
		    len - rc->rc_cmplen > HVN_RNDIS_CMPBUFSZ)
			printf("%s: RNDIS response %u too large: %u\n",
			    sc->sc_dev.dv_xname, id, len);
		else if (len > rc->rc_cmplen)
			memcpy(&rc->rc_cmpbuf, buf + rc->rc_cmplen,
			    len - rc->rc_cmplen);
		wakeup_one(rc);
	} else
		DPRINTF("%s: failed to complete RNDIS request id %u\n",
		    sc->sc_dev.dv_xname, id);
}

void
hvn_rndis_status(struct hvn_softc *sc, caddr_t buf, uint32_t len)
{
	uint32_t sta;

	memcpy(&sta, buf + RNDIS_HEADER_SIZE, sizeof(sta));
	switch (sta) {
	case RNDIS_STATUS_MEDIA_CONNECT:
		sc->sc_link_state = LINK_STATE_UP;
		break;
	case RNDIS_STATUS_MEDIA_DISCONNECT:
		sc->sc_link_state = LINK_STATE_DOWN;
		break;
	/* Ignore these */
	case RNDIS_STATUS_OFFLOAD_CURRENT_CONFIG:
		return;
	default:
		DPRINTF("%s: unhandled status %#x\n", sc->sc_dev.dv_xname, sta);
		return;
	}
	KERNEL_LOCK();
	hvn_link_status(sc);
	KERNEL_UNLOCK();
}

int
hvn_rndis_query(struct hvn_softc *sc, uint32_t oid, void *res, size_t *length)
{
	struct rndis_cmd *rc;
	struct rndis_query_req *req;
	struct rndis_query_comp *cmp;
	size_t olength = *length;
	int rv;

	rc = hvn_alloc_cmd(sc);

	bus_dmamap_sync(sc->sc_dmat, rc->rc_dmap, 0, PAGE_SIZE,
	    BUS_DMASYNC_PREREAD);

	rc->rc_req->msg_type = RNDIS_QUERY_MSG;
	rc->rc_req->msg_len = RNDIS_MESSAGE_SIZE(*req);
	rc->rc_cmplen = RNDIS_MESSAGE_SIZE(*cmp);
	rc->rc_id = atomic_inc_int_nv(&sc->sc_rndisrid);
	req = (struct rndis_query_req *)&rc->rc_req->msg;
	req->request_id = rc->rc_id;
	req->oid = oid;
	req->info_buffer_offset = sizeof(*req);

	bus_dmamap_sync(sc->sc_dmat, rc->rc_dmap, 0, PAGE_SIZE,
	    BUS_DMASYNC_PREWRITE);

	if ((rv = hvn_rndis_ctloutput(sc, rc, 500)) != 0) {
		DPRINTF("%s: QUERY_MSG failed, error %d\n",
		    sc->sc_dev.dv_xname, rv);
		hvn_free_cmd(sc, rc);
		return (rv);
	}

	cmp = (struct rndis_query_comp *)&rc->rc_cmp.msg;
	switch (cmp->status) {
	case RNDIS_STATUS_SUCCESS:
		if (cmp->info_buffer_length > olength) {
			rv = EINVAL;
			break;
		}
		memcpy(res, rc->rc_cmpbuf, cmp->info_buffer_length);
		*length = cmp->info_buffer_length;
		break;
	default:
		*length = 0;
		rv = EIO;
	}

	hvn_free_cmd(sc, rc);

	return (rv);
}

int
hvn_rndis_set(struct hvn_softc *sc, uint32_t oid, void *data, size_t length)
{
	struct rndis_cmd *rc;
	struct rndis_set_req *req;
	struct rndis_set_comp *cmp;
	int rv;

	rc = hvn_alloc_cmd(sc);

	bus_dmamap_sync(sc->sc_dmat, rc->rc_dmap, 0, PAGE_SIZE,
	    BUS_DMASYNC_PREREAD);

	rc->rc_req->msg_type = RNDIS_SET_MSG;
	rc->rc_req->msg_len = RNDIS_MESSAGE_SIZE(*req) + length;
	rc->rc_cmplen = RNDIS_MESSAGE_SIZE(*cmp);
	rc->rc_id = atomic_inc_int_nv(&sc->sc_rndisrid);
	req = (struct rndis_set_req *)&rc->rc_req->msg;
	memset(req, 0, sizeof(*req));
	req->request_id = rc->rc_id;
	req->oid = oid;
	req->info_buffer_offset = sizeof(*req);

	if (length > 0) {
		KASSERT(sizeof(*req) + length < sizeof(struct rndis));
		req->info_buffer_length = length;
		memcpy((caddr_t)(req + 1), data, length);
	}

	bus_dmamap_sync(sc->sc_dmat, rc->rc_dmap, 0, PAGE_SIZE,
	    BUS_DMASYNC_PREWRITE);

	if ((rv = hvn_rndis_ctloutput(sc, rc, 500)) != 0) {
		DPRINTF("%s: SET_MSG failed, error %u\n",
		    sc->sc_dev.dv_xname, rv);
		hvn_free_cmd(sc, rc);
		return (rv);
	}

	cmp = (struct rndis_set_comp *)&rc->rc_cmp.msg;
	if (cmp->status != RNDIS_STATUS_SUCCESS)
		rv = EIO;

	hvn_free_cmd(sc, rc);

	return (rv);
}

int
hvn_rndis_open(struct hvn_softc *sc)
{
	uint32_t filter;
	int rv;

	if (sc->sc_promisc)
		filter = NDIS_PACKET_TYPE_PROMISCUOUS;
	else
		filter = NDIS_PACKET_TYPE_BROADCAST |
		    NDIS_PACKET_TYPE_ALL_MULTICAST |
		    NDIS_PACKET_TYPE_DIRECTED;

	rv = hvn_rndis_set(sc, RNDIS_OID_GEN_CURRENT_PACKET_FILTER,
	    &filter, sizeof(filter));
	if (rv)
		DPRINTF("%s: failed to set RNDIS filter to %#x\n",
		    sc->sc_dev.dv_xname, filter);
	return (rv);
}

int
hvn_rndis_close(struct hvn_softc *sc)
{
	uint32_t filter = 0;
	int rv;

	rv = hvn_rndis_set(sc, RNDIS_OID_GEN_CURRENT_PACKET_FILTER,
	    &filter, sizeof(filter));
	if (rv)
		DPRINTF("%s: failed to clear RNDIS filter\n",
		    sc->sc_dev.dv_xname);
	return (rv);
}

void
hvn_rndis_detach(struct hvn_softc *sc)
{
	struct rndis_cmd *rc;
	struct rndis_halt_req *req;
	int rv;

	rc = hvn_alloc_cmd(sc);

	bus_dmamap_sync(sc->sc_dmat, rc->rc_dmap, 0, PAGE_SIZE,
	    BUS_DMASYNC_PREREAD);

	rc->rc_req->msg_type = RNDIS_HALT_MSG;
	rc->rc_req->msg_len = RNDIS_MESSAGE_SIZE(*req);
	rc->rc_id = atomic_inc_int_nv(&sc->sc_rndisrid);
	req = (struct rndis_halt_req *)&rc->rc_req->msg;
	req->request_id = rc->rc_id;

	bus_dmamap_sync(sc->sc_dmat, rc->rc_dmap, 0, PAGE_SIZE,
	    BUS_DMASYNC_PREWRITE);

	if ((rv = hvn_rndis_ctloutput(sc, rc, 500)) != 0)
		DPRINTF("%s: HALT_MSG failed, error %u\n",
		    sc->sc_dev.dv_xname, rv);

	hvn_free_cmd(sc, rc);
}
