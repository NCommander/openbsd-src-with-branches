/*	$OpenBSD: if_vlan.c,v 1.155 2016/03/18 02:40:04 dlg Exp $	*/

/*
 * Copyright 1998 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/net/if_vlan.c,v 1.16 2000/03/26 15:21:40 charnier Exp $
 */

/*
 * if_vlan.c - pseudo-device driver for IEEE 802.1Q virtual LANs.
 * This is sort of sneaky in the implementation, since
 * we need to pretend to be enough of an Ethernet implementation
 * to make arp work.  The way we do this is by telling everyone
 * that we are an Ethernet, and then catch the packets that
 * ether_output() left on our output queue when it calls
 * if_start(), rewrite them for use by the real outgoing interface,
 * and ask it to send them.
 *
 * Some devices support 802.1Q tag insertion in firmware.  The
 * vlan interface behavior changes when the IFCAP_VLAN_HWTAGGING
 * capability is set on the parent.  In this case, vlan_start()
 * will not modify the ethernet header.
 */

#include "mpw.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>
#include <sys/rwlock.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net/if_vlan_var.h>

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#define TAG_HASH_BITS		5
#define TAG_HASH_SIZE		(1 << TAG_HASH_BITS) 
#define TAG_HASH_MASK		(TAG_HASH_SIZE - 1)
#define TAG_HASH(tag)		(tag & TAG_HASH_MASK)
SRPL_HEAD(, ifvlan) *vlan_tagh, *svlan_tagh;
struct rwlock vlan_tagh_lk = RWLOCK_INITIALIZER("vlantag");

int	vlan_input(struct ifnet *, struct mbuf *, void *);
void	vlan_start(struct ifnet *ifp);
int	vlan_ioctl(struct ifnet *ifp, u_long cmd, caddr_t addr);
int	vlan_unconfig(struct ifnet *ifp, struct ifnet *newp);
int	vlan_config(struct ifvlan *, struct ifnet *, u_int16_t);
void	vlan_vlandev_state(void *);
void	vlanattach(int count);
int	vlan_set_promisc(struct ifnet *ifp);
int	vlan_clone_create(struct if_clone *, int);
int	vlan_clone_destroy(struct ifnet *);
void	vlan_ifdetach(void *);

int	vlan_multi_add(struct ifvlan *, struct ifreq *);
int	vlan_multi_del(struct ifvlan *, struct ifreq *);
void	vlan_multi_apply(struct ifvlan *, struct ifnet *, u_long);
void	vlan_multi_free(struct ifvlan *);

struct if_clone vlan_cloner =
    IF_CLONE_INITIALIZER("vlan", vlan_clone_create, vlan_clone_destroy);
struct if_clone svlan_cloner =
    IF_CLONE_INITIALIZER("svlan", vlan_clone_create, vlan_clone_destroy);

void vlan_ref(void *, void *);
void vlan_unref(void *, void *);

struct srpl_rc vlan_tagh_rc = SRPL_RC_INITIALIZER(vlan_ref, vlan_unref, NULL);

void
vlanattach(int count)
{
	u_int i;

	/* Normal VLAN */
	vlan_tagh = mallocarray(TAG_HASH_SIZE, sizeof(*vlan_tagh),
	    M_DEVBUF, M_NOWAIT);
	if (vlan_tagh == NULL)
		panic("vlanattach: hashinit");

	/* Service-VLAN for QinQ/802.1ad provider bridges */
	svlan_tagh = mallocarray(TAG_HASH_SIZE, sizeof(*svlan_tagh),
	    M_DEVBUF, M_NOWAIT);
	if (svlan_tagh == NULL)
		panic("vlanattach: hashinit");

	for (i = 0; i < TAG_HASH_SIZE; i++) {
		SRPL_INIT(&vlan_tagh[i]);
		SRPL_INIT(&svlan_tagh[i]);
	}

	if_clone_attach(&vlan_cloner);
	if_clone_attach(&svlan_cloner);
}

int
vlan_clone_create(struct if_clone *ifc, int unit)
{
	struct ifvlan	*ifv;
	struct ifnet	*ifp;

	ifv = malloc(sizeof(*ifv), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (ifv == NULL)
		return (ENOMEM);

	LIST_INIT(&ifv->vlan_mc_listhead);
	ifp = &ifv->ifv_if;
	ifp->if_softc = ifv;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "%s%d", ifc->ifc_name,
	    unit);
	/* NB: flags are not set here */
	/* NB: mtu is not set here */

	/* Special handling for the IEEE 802.1ad QinQ variant */
	if (strcmp("svlan", ifc->ifc_name) == 0)
		ifv->ifv_type = ETHERTYPE_QINQ;
	else
		ifv->ifv_type = ETHERTYPE_VLAN;

	refcnt_init(&ifv->ifv_refcnt);

	ifp->if_start = vlan_start;
	ifp->if_ioctl = vlan_ioctl;
	IFQ_SET_MAXLEN(&ifp->if_snd, 1);
	IFQ_SET_READY(&ifp->if_snd);
	if_attach(ifp);
	ether_ifattach(ifp);
	ifp->if_hdrlen = EVL_ENCAPLEN;

	return (0);
}

void
vlan_ref(void *null, void *v)
{
	struct ifvlan *ifv = v;

	refcnt_take(&ifv->ifv_refcnt);
}

void
vlan_unref(void *null, void *v)
{
	struct ifvlan *ifv = v;

	refcnt_rele_wake(&ifv->ifv_refcnt);
}

int
vlan_clone_destroy(struct ifnet *ifp)
{
	struct ifvlan	*ifv = ifp->if_softc;

	vlan_unconfig(ifp, NULL);
	ether_ifdetach(ifp);
	if_detach(ifp);
	refcnt_finalize(&ifv->ifv_refcnt, "vlanrefs");
	vlan_multi_free(ifv);
	free(ifv, M_DEVBUF, sizeof(*ifv));

	return (0);
}

void
vlan_ifdetach(void *ptr)
{
	struct ifvlan	*ifv = ptr;
	vlan_clone_destroy(&ifv->ifv_if);
}

static inline int
vlan_mplstunnel(int ifidx)
{
#if NMPW > 0
	struct ifnet *ifp;
	int rv = 0;

	ifp = if_get(ifidx);
	if (ifp != NULL) {
		rv = ifp->if_type == IFT_MPLSTUNNEL;
		if_put(ifp);
	}
	return (rv);
#else
	return (0);
#endif
}

void
vlan_start(struct ifnet *ifp)
{
	struct ifvlan   *ifv;
	struct ifnet	*ifp0;
	struct mbuf	*m;
	uint8_t		 prio;

	ifv = ifp->if_softc;
	ifp0 = if_get(ifv->ifv_p);
	if (ifp0 == NULL || (ifp0->if_flags & (IFF_UP|IFF_RUNNING)) !=
	    (IFF_UP|IFF_RUNNING)) {
		ifq_purge(&ifp->if_snd);
		goto leave;
	}

	for (;;) {
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap_ether(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif /* NBPFILTER > 0 */


		/* IEEE 802.1p has prio 0 and 1 swapped */
		prio = m->m_pkthdr.pf.prio;
		if (prio <= 1)
			prio = !prio;

		/*
		 * If this packet came from a pseudowire it means it already
		 * has all tags it needs, so just output it.
		 */
		if (vlan_mplstunnel(m->m_pkthdr.ph_ifidx)) {
			/* NOTHING */

		/*
		 * If the underlying interface cannot do VLAN tag insertion
		 * itself, create an encapsulation header.
		 */
		} else if ((ifp0->if_capabilities & IFCAP_VLAN_HWTAGGING) &&
		    (ifv->ifv_type == ETHERTYPE_VLAN)) {
			m->m_pkthdr.ether_vtag = ifv->ifv_tag +
			    (prio << EVL_PRIO_BITS);
			m->m_flags |= M_VLANTAG;
		} else {
			m = vlan_inject(m, ifv->ifv_type, ifv->ifv_tag |
                            (prio << EVL_PRIO_BITS));
			if (m == NULL) {
				ifp->if_oerrors++;
				continue;
			}
		}

		if (if_enqueue(ifp0, m)) {
			ifp->if_oerrors++;
			continue;
		}
		ifp->if_opackets++;
	}

leave:
	if_put(ifp0);
}

struct mbuf *
vlan_inject(struct mbuf *m, uint16_t type, uint16_t tag)
{
	struct ether_vlan_header evh;

	m_copydata(m, 0, ETHER_HDR_LEN, (caddr_t)&evh);
	evh.evl_proto = evh.evl_encap_proto;
	evh.evl_encap_proto = htons(type);
	evh.evl_tag = htons(tag);
	m_adj(m, ETHER_HDR_LEN);
	M_PREPEND(m, sizeof(evh), M_DONTWAIT);
	if (m == NULL)
		return (NULL);

	m_copyback(m, 0, sizeof(evh), &evh, M_NOWAIT);
	CLR(m->m_flags, M_VLANTAG);

	return (m);
 }

/*
 * vlan_input() returns 1 if it has consumed the packet, 0 otherwise.
 */
int
vlan_input(struct ifnet *ifp0, struct mbuf *m, void *cookie)
{
	struct ifvlan			*ifv;
	struct ether_vlan_header	*evl;
	struct ether_header		*eh;
	SRPL_HEAD(, ifvlan)		*tagh, *list;
	struct srpl_iter		 i;
	u_int				 tag;
	struct mbuf_list		 ml = MBUF_LIST_INITIALIZER();
	u_int16_t			 etype;

	eh = mtod(m, struct ether_header *);
	etype = ntohs(eh->ether_type);

	if (m->m_flags & M_VLANTAG) {
		etype = ETHERTYPE_VLAN;
		tagh = vlan_tagh;
	} else if ((etype == ETHERTYPE_VLAN) || (etype == ETHERTYPE_QINQ)) {
		if (m->m_len < EVL_ENCAPLEN &&
		    (m = m_pullup(m, EVL_ENCAPLEN)) == NULL) {
			ifp0->if_ierrors++;
			return (1);
		}

		evl = mtod(m, struct ether_vlan_header *);
		m->m_pkthdr.ether_vtag = ntohs(evl->evl_tag);
		tagh = etype == ETHERTYPE_QINQ ? svlan_tagh : vlan_tagh;
	} else {
		/* Skip non-VLAN packets. */
		return (0);
	}

	/* From now on ether_vtag is fine */
	tag = EVL_VLANOFTAG(m->m_pkthdr.ether_vtag);
	m->m_pkthdr.pf.prio = EVL_PRIOFTAG(m->m_pkthdr.ether_vtag);

	/* IEEE 802.1p has prio 0 and 1 swapped */
	if (m->m_pkthdr.pf.prio <= 1)
		m->m_pkthdr.pf.prio = !m->m_pkthdr.pf.prio;

	list = &tagh[TAG_HASH(tag)];
	SRPL_FOREACH(ifv, list, &i, ifv_list) {
		if (ifp0->if_index == ifv->ifv_p && tag == ifv->ifv_tag &&
		    etype == ifv->ifv_type)
			break;
	}

	if (ifv == NULL) {
		ifp0->if_noproto++;
		goto drop;
	}

	if ((ifv->ifv_if.if_flags & (IFF_UP|IFF_RUNNING)) !=
	    (IFF_UP|IFF_RUNNING))
		goto drop;

	/*
	 * Drop promiscuously received packets if we are not in
	 * promiscuous mode.
	 */
	if (!ETHER_IS_MULTICAST(eh->ether_dhost) &&
	    (ifp0->if_flags & IFF_PROMISC) &&
	    (ifv->ifv_if.if_flags & IFF_PROMISC) == 0) {
		if (bcmp(&ifv->ifv_ac.ac_enaddr, eh->ether_dhost,
		    ETHER_ADDR_LEN))
			goto drop;
	}

	/*
	 * Having found a valid vlan interface corresponding to
	 * the given source interface and vlan tag, remove the
	 * encapsulation.
	 */
	if (m->m_flags & M_VLANTAG) {
		m->m_flags &= ~M_VLANTAG;
	} else {
		eh->ether_type = evl->evl_proto;
		memmove((char *)eh + EVL_ENCAPLEN, eh, sizeof(*eh));
		m_adj(m, EVL_ENCAPLEN);
	}

	ml_enqueue(&ml, m);
	if_input(&ifv->ifv_if, &ml);
	SRPL_LEAVE(&i, ifv);
	return (1);

drop:
	SRPL_LEAVE(&i, ifv);
	m_freem(m);
	return (1);
}

int
vlan_config(struct ifvlan *ifv, struct ifnet *ifp0, u_int16_t tag)
{
	struct sockaddr_dl	*sdl1, *sdl2;
	SRPL_HEAD(, ifvlan)	*tagh, *list;
	u_int			 flags;

	if (ifp0->if_type != IFT_ETHER)
		return EPROTONOSUPPORT;
	if (ifp0->if_index == ifv->ifv_p && ifv->ifv_tag == tag) /* noop */
		return (0);

	/* Remember existing interface flags and reset the interface */
	flags = ifv->ifv_flags;
	vlan_unconfig(&ifv->ifv_if, ifp0);
	ifv->ifv_p = ifp0->if_index;
	ifv->ifv_if.if_baudrate = ifp0->if_baudrate;

	if (ifp0->if_capabilities & IFCAP_VLAN_MTU) {
		ifv->ifv_if.if_mtu = ifp0->if_mtu;
		ifv->ifv_if.if_hardmtu = ifp0->if_hardmtu;
	} else {
		ifv->ifv_if.if_mtu = ifp0->if_mtu - EVL_ENCAPLEN;
		ifv->ifv_if.if_hardmtu = ifp0->if_hardmtu - EVL_ENCAPLEN;
	}

	ifv->ifv_if.if_flags = ifp0->if_flags &
	    (IFF_UP | IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST);

	/* Reset promisc mode on the interface and its parent */
	if (flags & IFVF_PROMISC) {
		ifv->ifv_if.if_flags |= IFF_PROMISC;
		vlan_set_promisc(&ifv->ifv_if);
	}

	if (ifv->ifv_type != ETHERTYPE_VLAN) {
		/*
		 * Hardware offload only works with the default VLAN
		 * ethernet type (0x8100).
		 */
		ifv->ifv_if.if_capabilities = 0;
	} else if (ifp0->if_capabilities & IFCAP_VLAN_HWTAGGING) {
		/*
		 * If the parent interface can do hardware-assisted
		 * VLAN encapsulation, then propagate its hardware-
		 * assisted checksumming flags.
		 *
		 * If the card cannot handle hardware tagging, it cannot
		 * possibly compute the correct checksums for tagged packets.
		 */
		ifv->ifv_if.if_capabilities = ifp0->if_capabilities &
		    IFCAP_CSUM_MASK;
	}

	/*
	 * Set up our ``Ethernet address'' to reflect the underlying
	 * physical interface's.
	 */
	sdl1 = ifv->ifv_if.if_sadl;
	sdl2 = ifp0->if_sadl;
	sdl1->sdl_type = IFT_ETHER;
	sdl1->sdl_alen = ETHER_ADDR_LEN;
	bcopy(LLADDR(sdl2), LLADDR(sdl1), ETHER_ADDR_LEN);
	bcopy(LLADDR(sdl2), ifv->ifv_ac.ac_enaddr, ETHER_ADDR_LEN);

	ifv->ifv_tag = tag;

	/* Register callback for physical link state changes */
	ifv->lh_cookie = hook_establish(ifp0->if_linkstatehooks, 1,
	    vlan_vlandev_state, ifv);

	/* Register callback if parent wants to unregister */
	ifv->dh_cookie = hook_establish(ifp0->if_detachhooks, 0,
	    vlan_ifdetach, ifv);

	vlan_multi_apply(ifv, ifp0, SIOCADDMULTI);

	vlan_vlandev_state(ifv);

	/* Change input handler of the physical interface. */
	if_ih_insert(ifp0, vlan_input, NULL);

	tagh = ifv->ifv_type == ETHERTYPE_QINQ ? svlan_tagh : vlan_tagh;
	list = &tagh[TAG_HASH(tag)];

	rw_enter_write(&vlan_tagh_lk);
	SRPL_INSERT_HEAD_LOCKED(&vlan_tagh_rc, list, ifv, ifv_list);
	rw_exit_write(&vlan_tagh_lk);

	return (0);
}

int
vlan_unconfig(struct ifnet *ifp, struct ifnet *newifp0)
{
	struct sockaddr_dl	*sdl;
	struct ifvlan		*ifv;
	SRPL_HEAD(, ifvlan)	*tagh, *list;
	struct ifnet		*ifp0;

	ifv = ifp->if_softc;
	ifp0 = if_get(ifv->ifv_p);
	if (ifp0 == NULL)
		goto disconnect;

	/* Unset promisc mode on the interface and its parent */
	if (ifv->ifv_flags & IFVF_PROMISC) {
		ifp->if_flags &= ~IFF_PROMISC;
		vlan_set_promisc(ifp);
	}

	tagh = ifv->ifv_type == ETHERTYPE_QINQ ? svlan_tagh : vlan_tagh;
	list = &tagh[TAG_HASH(ifv->ifv_tag)];

	rw_enter_write(&vlan_tagh_lk);
	SRPL_REMOVE_LOCKED(&vlan_tagh_rc, list, ifv, ifvlan, ifv_list);
	rw_exit_write(&vlan_tagh_lk);

	/* Restore previous input handler. */
	if_ih_remove(ifp0, vlan_input, NULL);

	hook_disestablish(ifp0->if_linkstatehooks, ifv->lh_cookie);
	hook_disestablish(ifp0->if_detachhooks, ifv->dh_cookie);
	/* Reset link state */
	if (newifp0 != NULL) {
		ifp->if_link_state = LINK_STATE_INVALID;
		if_link_state_change(ifp);
	}

	/*
 	 * Since the interface is being unconfigured, we need to
	 * empty the list of multicast groups that we may have joined
	 * while we were alive and remove them from the parent's list
	 * as well.
	 */
	vlan_multi_apply(ifv, ifp0, SIOCDELMULTI);

disconnect:
	/* Disconnect from parent. */
	ifv->ifv_p = 0;
	ifv->ifv_if.if_mtu = ETHERMTU;
	ifv->ifv_if.if_hardmtu = ETHERMTU;
	ifv->ifv_flags = 0;

	/* Clear our MAC address. */
	sdl = ifv->ifv_if.if_sadl;
	sdl->sdl_type = IFT_ETHER;
	sdl->sdl_alen = ETHER_ADDR_LEN;
	bzero(LLADDR(sdl), ETHER_ADDR_LEN);
	bzero(ifv->ifv_ac.ac_enaddr, ETHER_ADDR_LEN);

	if_put(ifp0);

	return (0);
}

void
vlan_vlandev_state(void *v)
{
	struct ifvlan	*ifv = v;
	struct ifnet	*ifp0;
	int		 link_state = LINK_STATE_DOWN;
	uint64_t	 baudrate = 0;

	ifp0 = if_get(ifv->ifv_p);
	if (ifp0 != NULL) {
		link_state = ifp0->if_link_state;
		baudrate = ifp0->if_baudrate;
	}
	if_put(ifp0);

	if (ifv->ifv_if.if_link_state == link_state)
		return;

	ifv->ifv_if.if_link_state = link_state;
	ifv->ifv_if.if_baudrate = baudrate;
	if_link_state_change(&ifv->ifv_if);
}

int
vlan_set_promisc(struct ifnet *ifp)
{
	struct ifvlan	*ifv = ifp->if_softc;
	struct ifnet	*ifp0;
	int		 error = 0;

	ifp0 = if_get(ifv->ifv_p);
	if (ifp0 == NULL)
		goto leave;

	if ((ifp->if_flags & IFF_PROMISC) != 0) {
		if ((ifv->ifv_flags & IFVF_PROMISC) == 0)
			if ((error = ifpromisc(ifp0, 1)) == 0)
				ifv->ifv_flags |= IFVF_PROMISC;
	} else {
		if ((ifv->ifv_flags & IFVF_PROMISC) != 0)
			if ((error = ifpromisc(ifp0, 0)) == 0)
				ifv->ifv_flags &= ~IFVF_PROMISC;
	}

leave:
	if_put(ifp0);

	return (0);
}

int
vlan_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct proc	*p = curproc;	/* XXX */
	struct ifaddr	*ifa;
	struct ifnet	*ifp0;
	struct ifreq	*ifr;
	struct ifvlan	*ifv;
	struct vlanreq	 vlr;
	int		 error = 0, s;

	ifr = (struct ifreq *)data;
	ifa = (struct ifaddr *)data;
	ifv = ifp->if_softc;

	switch (cmd) {
	case SIOCSIFADDR:
		if (ifv->ifv_p != 0)
			ifp->if_flags |= IFF_UP;
		else
			error = EINVAL;
		break;

	case SIOCSIFMTU:
		if (ifv->ifv_p != 0) {
			if (ifr->ifr_mtu < ETHERMIN ||
			    ifr->ifr_mtu > ifv->ifv_if.if_hardmtu)
				error = EINVAL;
			else
				ifp->if_mtu = ifr->ifr_mtu;
		} else
			error = EINVAL;

		break;

	case SIOCSETVLAN:
		if ((error = suser(p, 0)) != 0)
			break;
		if ((error = copyin(ifr->ifr_data, &vlr, sizeof vlr)))
			break;
		if (vlr.vlr_parent[0] == '\0') {
			s = splnet();
			vlan_unconfig(ifp, NULL);
			if (ifp->if_flags & IFF_UP)
				if_down(ifp);
			ifp->if_flags &= ~IFF_RUNNING;
			splx(s);
			break;
		}
		ifp0 = ifunit(vlr.vlr_parent);
		if (ifp0 == NULL) {
			error = ENOENT;
			break;
		}
		/*
		 * Don't let the caller set up a VLAN tag with
		 * anything except VLID bits.
		 */
		if (vlr.vlr_tag & ~EVL_VLID_MASK) {
			error = EINVAL;
			break;
		}
		error = vlan_config(ifv, ifp0, vlr.vlr_tag);
		if (error)
			break;
		ifp->if_flags |= IFF_RUNNING;

		/* Update promiscuous mode, if necessary. */
		vlan_set_promisc(ifp);
		break;
		
	case SIOCGETVLAN:
		bzero(&vlr, sizeof vlr);
		ifp0 = if_get(ifv->ifv_p);
		if (ifp0) {
			snprintf(vlr.vlr_parent, sizeof(vlr.vlr_parent),
			    "%s", ifp0->if_xname);
			vlr.vlr_tag = ifv->ifv_tag;
		}
		if_put(ifp0);
		error = copyout(&vlr, ifr->ifr_data, sizeof vlr);
		break;
	case SIOCSIFFLAGS:
		/*
		 * For promiscuous mode, we enable promiscuous mode on
		 * the parent if we need promiscuous on the VLAN interface.
		 */
		if (ifv->ifv_p != 0)
			error = vlan_set_promisc(ifp);
		break;

	case SIOCADDMULTI:
		error = vlan_multi_add(ifv, ifr);
		break;

	case SIOCDELMULTI:
		error = vlan_multi_del(ifv, ifr);
		break;
	default:
		error = ENOTTY;
	}
	return error;
}


int
vlan_multi_add(struct ifvlan *ifv, struct ifreq *ifr)
{
	struct ifnet		*ifp0;
	struct vlan_mc_entry	*mc;
	u_int8_t		 addrlo[ETHER_ADDR_LEN], addrhi[ETHER_ADDR_LEN];
	int			 error;

	error = ether_addmulti(ifr, &ifv->ifv_ac);
	if (error != ENETRESET)
		return (error);

	/*
	 * This is new multicast address.  We have to tell parent
	 * about it.  Also, remember this multicast address so that
	 * we can delete them on unconfigure.
	 */
	if ((mc = malloc(sizeof(*mc), M_DEVBUF, M_NOWAIT)) == NULL) {
		error = ENOMEM;
		goto alloc_failed;
	}

	/*
	 * As ether_addmulti() returns ENETRESET, following two
	 * statement shouldn't fail.
	 */
	(void)ether_multiaddr(&ifr->ifr_addr, addrlo, addrhi);
	ETHER_LOOKUP_MULTI(addrlo, addrhi, &ifv->ifv_ac, mc->mc_enm);
	memcpy(&mc->mc_addr, &ifr->ifr_addr, ifr->ifr_addr.sa_len);
	LIST_INSERT_HEAD(&ifv->vlan_mc_listhead, mc, mc_entries);

	ifp0 = if_get(ifv->ifv_p);
	error = (ifp0 == NULL) ? 0 :
	    (*ifp0->if_ioctl)(ifp0, SIOCADDMULTI, (caddr_t)ifr);
	if_put(ifp0);

	if (error != 0) 
		goto ioctl_failed;

	return (error);

 ioctl_failed:
	LIST_REMOVE(mc, mc_entries);
	free(mc, M_DEVBUF, sizeof(*mc));
 alloc_failed:
	(void)ether_delmulti(ifr, &ifv->ifv_ac);

	return (error);
}

int
vlan_multi_del(struct ifvlan *ifv, struct ifreq *ifr)
{
	struct ifnet		*ifp0;
	struct ether_multi	*enm;
	struct vlan_mc_entry	*mc;
	u_int8_t		 addrlo[ETHER_ADDR_LEN], addrhi[ETHER_ADDR_LEN];
	int			 error;

	/*
	 * Find a key to lookup vlan_mc_entry.  We have to do this
	 * before calling ether_delmulti for obvious reason.
	 */
	if ((error = ether_multiaddr(&ifr->ifr_addr, addrlo, addrhi)) != 0)
		return (error);
	ETHER_LOOKUP_MULTI(addrlo, addrhi, &ifv->ifv_ac, enm);
	if (enm == NULL)
		return (EINVAL);

	LIST_FOREACH(mc, &ifv->vlan_mc_listhead, mc_entries) {
		if (mc->mc_enm == enm)
			break;
	}

	/* We won't delete entries we didn't add */
	if (mc == NULL)
		return (EINVAL);

	error = ether_delmulti(ifr, &ifv->ifv_ac);
	if (error != ENETRESET)
		return (error);

	if (!ISSET(ifv->ifv_if.if_flags, IFF_RUNNING))
		goto forget;

	ifp0 = if_get(ifv->ifv_p);
	error = (ifp0 == NULL) ? 0 :
	    (*ifp0->if_ioctl)(ifp0, SIOCDELMULTI, (caddr_t)ifr);
	if_put(ifp0);

	if (error != 0) {
		(void)ether_addmulti(ifr, &ifv->ifv_ac);
		return (error);
	}

forget:
	/* forget about this address */
	LIST_REMOVE(mc, mc_entries);
	free(mc, M_DEVBUF, sizeof(*mc));

	return (0);
}

void
vlan_multi_apply(struct ifvlan *ifv, struct ifnet *ifp0, u_long cmd)
{
	struct vlan_mc_entry	*mc;
	union {
		struct ifreq ifreq;
		struct {
			char			ifr_name[IFNAMSIZ];
			struct sockaddr_storage	ifr_ss;
		} ifreq_storage;
	} ifreq;
	struct ifreq	*ifr = &ifreq.ifreq;

	memcpy(ifr->ifr_name, ifp0->if_xname, IFNAMSIZ);
	LIST_FOREACH(mc, &ifv->vlan_mc_listhead, mc_entries) {
		memcpy(&ifr->ifr_addr, &mc->mc_addr, mc->mc_addr.ss_len);

		(void)(*ifp0->if_ioctl)(ifp0, cmd, (caddr_t)ifr);
	}
}

void
vlan_multi_free(struct ifvlan *ifv)
{
	struct vlan_mc_entry	*mc;

	while ((mc = LIST_FIRST(&ifv->vlan_mc_listhead)) != NULL) {
		LIST_REMOVE(mc, mc_entries);
		free(mc, M_DEVBUF, sizeof(*mc));
	}
}
