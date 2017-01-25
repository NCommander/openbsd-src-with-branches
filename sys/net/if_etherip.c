/*	$OpenBSD: if_etherip.c,v 1.12 2017/01/23 11:37:29 mpi Exp $	*/
/*
 * Copyright (c) 2015 Kazuya GODA <goda@openbsd.org>
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

#include "bpfilter.h"
#include "pf.h"
#include "gif.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip_ether.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#if NPF > 0
#include <net/pfvar.h>
#endif

#include <net/if_etherip.h>

struct etherip_softc {
	struct arpcom sc_ac;
	struct ifmedia sc_media;
	unsigned int sc_rdomain;
	struct sockaddr_storage sc_src;
	struct sockaddr_storage sc_dst;
	LIST_ENTRY(etherip_softc) sc_entry;
};

LIST_HEAD(, etherip_softc) etherip_softc_list;

#if 0
/*
 * TODO:
 *   At this stage, etherip_allow and etheripstat are defined
 *   at netinet/ip_ether.c. When implementation of etherip is
 *   removed from gif(4), there are moved here.
 */

/*
 * We can control the acceptance of EtherIP packets by altering the sysctl
 * net.inet.etherip.allow value. Zero means drop them, all else is acceptance.
 */
int etherip_allow = 0;

struct etheripstat etheripstat;
#endif

void etheripattach(int);
int etherip_clone_create(struct if_clone *, int);
int etherip_clone_destroy(struct ifnet *);
int etherip_ioctl(struct ifnet *, u_long, caddr_t);
void etherip_start(struct ifnet *);
int etherip_media_change(struct ifnet *);
void etherip_media_status(struct ifnet *, struct ifmediareq *);
int etherip_set_tunnel_addr(struct ifnet *, struct sockaddr_storage *,
   struct sockaddr_storage *);

struct if_clone	etherip_cloner = IF_CLONE_INITIALIZER("etherip",
    etherip_clone_create, etherip_clone_destroy);


void
etheripattach(int count)
{
	if_clone_attach(&etherip_cloner);
}

int
etherip_clone_create(struct if_clone *ifc, int unit)
{
	struct ifnet *ifp;
	struct etherip_softc *sc;

	if ((sc = malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT|M_ZERO)) == NULL)
		return ENOMEM;

	ifp = &sc->sc_ac.ac_if;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "etherip%d", unit);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ether_fakeaddr(ifp);

	ifp->if_softc = sc;
	ifp->if_ioctl = etherip_ioctl;
	ifp->if_start = etherip_start;
	ifp->if_xflags = IFXF_CLONED;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);

	ifp->if_capabilities = IFCAP_VLAN_MTU;

	ifmedia_init(&sc->sc_media, 0, etherip_media_change,
	    etherip_media_status);
	ifmedia_add(&sc->sc_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER | IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);

	LIST_INSERT_HEAD(&etherip_softc_list, sc, sc_entry);

	return 0;
}

int
etherip_clone_destroy(struct ifnet *ifp)
{
	struct etherip_softc *sc = ifp->if_softc;

	LIST_REMOVE(sc, sc_entry);

	ifmedia_delete_instance(&sc->sc_media, IFM_INST_ANY);
	ether_ifdetach(ifp);
	if_detach(ifp);
	free(sc, M_DEVBUF, sizeof(*sc));

	return 0;
}

int
etherip_media_change(struct ifnet *ifp)
{
	return 0;
}

void
etherip_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	imr->ifm_active = IFM_ETHER | IFM_AUTO;
	imr->ifm_status = IFM_AVALID | IFM_ACTIVE;
}

void
etherip_start(struct ifnet *ifp)
{
	struct etherip_softc *sc = ifp->if_softc;
	struct mbuf *m;
	int error;

	for (;;) {
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
		if (sc->sc_src.ss_family == AF_UNSPEC ||
		    sc->sc_dst.ss_family == AF_UNSPEC) {
			m_freem(m);
			continue;
		}

		switch (sc->sc_src.ss_family) {
		case AF_INET:
			error = ip_etherip_output(ifp, m);
			break;
#ifdef INET6
		case AF_INET6:
			error = ip6_etherip_output(ifp, m);
			break;
#endif
		default:
			unhandled_af(sc->sc_src.ss_family);
		}

		if (error)
			ifp->if_oerrors++;
	}

}


int
etherip_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct etherip_softc *sc = ifp->if_softc;
	struct if_laddrreq *lifr = (struct if_laddrreq *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	struct sockaddr_storage *src, *dst;
	struct proc *p = curproc;
	int error = 0;

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/* FALLTHROUGH */

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP)
			ifp->if_flags |= IFF_RUNNING;
		else
			ifp->if_flags &= ~IFF_RUNNING;

		break;

	case SIOCSLIFPHYRTABLE:
		if ((error = suser(p, 0)) != 0)
			break;

		if (ifr->ifr_rdomainid < 0 ||
		    ifr->ifr_rdomainid > RT_TABLEID_MAX ||
		    !rtable_exists(ifr->ifr_rdomainid)) {
			error = EINVAL;
			break;
		}
		sc->sc_rdomain = ifr->ifr_rdomainid;
		break;

	case SIOCGLIFPHYRTABLE:
		ifr->ifr_rdomainid = sc->sc_rdomain;
		break;

	case SIOCSLIFPHYADDR:
		if ((error = suser(p, 0)) != 0)
			break;

		src = &lifr->addr;
		dst = &lifr->dstaddr;
		if (src->ss_family == AF_UNSPEC || dst->ss_family == AF_UNSPEC)
			return EADDRNOTAVAIL;

		switch (src->ss_family) {
		case AF_INET:
			if (src->ss_len != sizeof(struct sockaddr_in) ||
			    dst->ss_len != sizeof(struct sockaddr_in))
				return EINVAL;
			break;
#ifdef INET6
		case AF_INET6:
			if (src->ss_len != sizeof(struct sockaddr_in6) ||
			    dst->ss_len != sizeof(struct sockaddr_in6))
				return EINVAL;
			break;
#endif
		default:
			return EAFNOSUPPORT;
		}

		error = etherip_set_tunnel_addr(ifp, src, dst);
		break;

	case SIOCDIFPHYADDR:
		if ((error = suser(p, 0)) != 0)
			break;

		ifp->if_flags &= ~IFF_RUNNING;
		memset(&sc->sc_src, 0, sizeof(sc->sc_src));
		memset(&sc->sc_dst, 0, sizeof(sc->sc_dst));
		break;

	case SIOCGLIFPHYADDR:
		if (sc->sc_dst.ss_family == AF_UNSPEC)
			return EADDRNOTAVAIL;

		memset(&lifr->addr, 0, sizeof(lifr->addr));
		memset(&lifr->dstaddr, 0, sizeof(lifr->dstaddr));
		memcpy(&lifr->addr, &sc->sc_src, sc->sc_src.ss_len);
		memcpy(&lifr->dstaddr, &sc->sc_dst, sc->sc_dst.ss_len);

		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
		break;
	}

	return error;
}

int
etherip_set_tunnel_addr(struct ifnet *ifp, struct sockaddr_storage *src,
    struct sockaddr_storage *dst)
{
	struct etherip_softc *sc, *tsc;
	int error = 0;

	sc  = ifp->if_softc;

	LIST_FOREACH(tsc, &etherip_softc_list, sc_entry) {
		if (tsc == sc)
			continue;

		if (tsc->sc_src.ss_family != src->ss_family ||
		    tsc->sc_dst.ss_family != dst->ss_family ||
		    tsc->sc_src.ss_len != src->ss_len ||
		    tsc->sc_dst.ss_len != dst->ss_len)
			continue;

		if (tsc->sc_rdomain == sc->sc_rdomain &&
		    memcmp(&tsc->sc_dst, dst, dst->ss_len) == 0 &&
		    memcmp(&tsc->sc_src, src, src->ss_len) == 0) {
			error = EADDRNOTAVAIL;
			goto out;
		}
	}

	memcpy(&sc->sc_src, src, src->ss_len);
	memcpy(&sc->sc_dst, dst, dst->ss_len);
out:
	return error;
}

int
ip_etherip_output(struct ifnet *ifp, struct mbuf *m)
{
	struct etherip_softc *sc = (struct etherip_softc *)ifp->if_softc;
	struct sockaddr_in *src, *dst;
	struct etherip_header *eip;
	struct ip *ip;

	src = (struct sockaddr_in *)&sc->sc_src;
	dst = (struct sockaddr_in *)&sc->sc_dst;

	if (src == NULL || dst == NULL ||
	    src->sin_family != AF_INET || dst->sin_family != AF_INET) {
		m_freem(m);
		return EAFNOSUPPORT;
	}
	if (dst->sin_addr.s_addr == INADDR_ANY) {
		m_freem(m);
		return ENETUNREACH;
	}

	m->m_flags &= ~(M_BCAST|M_MCAST);

	M_PREPEND(m, sizeof(struct etherip_header), M_DONTWAIT);
	if (m == NULL) {
		etheripstat.etherip_adrops++;
		return ENOBUFS;
	}
	eip = mtod(m, struct etherip_header *);
	eip->eip_ver = ETHERIP_VERSION;
	eip->eip_res = 0;
	eip->eip_pad = 0;

	M_PREPEND(m, sizeof(struct ip), M_DONTWAIT);
	if (m == NULL) {
		etheripstat.etherip_adrops++;
		return ENOBUFS;
	}
	ip = mtod(m, struct ip *);
	memset(ip, 0, sizeof(struct ip));

	ip->ip_v = IPVERSION;
	ip->ip_hl = sizeof(struct ip) >> 2;
	ip->ip_id = htons(ip_randomid());
	ip->ip_tos = IPTOS_LOWDELAY;
	ip->ip_p = IPPROTO_ETHERIP;
	ip->ip_len = htons(m->m_pkthdr.len);
	ip->ip_ttl = IPDEFTTL;
	ip->ip_src = src->sin_addr;
	ip->ip_dst = dst->sin_addr;

	m->m_pkthdr.ph_rtableid = sc->sc_rdomain;

#if NPF > 0
	pf_pkt_addr_changed(m);
#endif
	etheripstat.etherip_opackets++;
	etheripstat.etherip_obytes += (m->m_pkthdr.len -
	    (sizeof(struct ip) + sizeof(struct etherip_header)));

	return ip_output(m, NULL, NULL, IP_RAWOUTPUT, NULL, NULL, 0);
}

void
ip_etherip_input(struct mbuf *m, int off, int proto)
{
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	struct etherip_softc *sc;
	const struct ip *ip;
	struct etherip_header *eip;
	struct sockaddr_in *src, *dst;
	struct ifnet *ifp = NULL;

	ip = mtod(m, struct ip *);

	if (ip->ip_p != IPPROTO_ETHERIP) {
		m_freem(m);
		ipstat_inc(ips_noproto);
		return;
	}

	if (!etherip_allow) {
		m_freem(m);
		etheripstat.etherip_pdrops++;
		return;
	}

	LIST_FOREACH(sc, &etherip_softc_list, sc_entry) {
		if (sc->sc_src.ss_family != AF_INET ||
		    sc->sc_dst.ss_family != AF_INET)
			continue;

		src = (struct sockaddr_in *)&sc->sc_src;
		dst = (struct sockaddr_in *)&sc->sc_dst;

		if (sc->sc_rdomain != rtable_l2(m->m_pkthdr.ph_rtableid) ||
		    src->sin_addr.s_addr != ip->ip_dst.s_addr ||
		    dst->sin_addr.s_addr != ip->ip_src.s_addr)
			continue;

		ifp = &sc->sc_ac.ac_if;
		break;
	}

	if (ifp == NULL) {
#if NGIF > 0
		/*
		 * This path is nessesary for gif(4) and etherip(4) coexistence.
		 * This is tricky but the path will be removed soon when
		 * implementation of etherip is removed from gif(4).
		 */
		etherip_input(m, off, proto);
#else
		etheripstat.etherip_noifdrops++;
		m_freem(m);
#endif /* NGIF */
		return;
	}

	m_adj(m, off);
	m = m_pullup(m, sizeof(struct etherip_header));
	if (m == NULL) {
		etheripstat.etherip_adrops++;
		return;
	}

	eip = mtod(m, struct etherip_header *);
	if (eip->eip_ver != ETHERIP_VERSION || eip->eip_pad) {
		etheripstat.etherip_adrops++;
		m_freem(m);
		return;
	}

	etheripstat.etherip_ipackets++;
	etheripstat.etherip_ibytes += (m->m_pkthdr.len -
	    sizeof(struct etherip_header));

	m_adj(m, sizeof(struct etherip_header));
	m = m_pullup(m, sizeof(struct ether_header));
	if (m == NULL) {
		etheripstat.etherip_adrops++;
		return;
	}
	m->m_flags &= ~(M_BCAST|M_MCAST);

#if NPF > 0
	pf_pkt_addr_changed(m);
#endif

	ml_enqueue(&ml, m);
	if_input(ifp, &ml);
}

#ifdef INET6
int
ip6_etherip_output(struct ifnet *ifp, struct mbuf *m)
{
	struct etherip_softc *sc = (struct etherip_softc *)ifp->if_softc;
	struct sockaddr_in6 *src, *dst;
	struct etherip_header *eip;
	struct ip6_hdr *ip6;
	int error;

	src = (struct sockaddr_in6 *)&sc->sc_src;
	dst = (struct sockaddr_in6 *)&sc->sc_dst;

	if (src == NULL || dst == NULL ||
	    src->sin6_family != AF_INET6 || dst->sin6_family != AF_INET6) {
		error = EAFNOSUPPORT;
		goto drop;
	}
	if (IN6_IS_ADDR_UNSPECIFIED(&dst->sin6_addr)) {
		error = ENETUNREACH;
		goto drop;
	}

	m->m_flags &= ~(M_BCAST|M_MCAST);

	M_PREPEND(m, sizeof(struct etherip_header), M_DONTWAIT);
	if (m == NULL) {
		etheripstat.etherip_adrops++;
		return ENOBUFS;
	}
	eip = mtod(m, struct etherip_header *);
	eip->eip_ver = ETHERIP_VERSION;
	eip->eip_res = 0;
	eip->eip_pad = 0;

	M_PREPEND(m, sizeof(struct ip6_hdr), M_DONTWAIT);
	if (m == NULL) {
		etheripstat.etherip_adrops++;
		return ENOBUFS;
	}
	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_flow = 0;
	ip6->ip6_vfc &= ~IPV6_VERSION_MASK;
	ip6->ip6_vfc |= IPV6_VERSION;
	ip6->ip6_nxt  = IPPROTO_ETHERIP;
	ip6->ip6_hlim = ip6_defhlim;
	ip6->ip6_plen = htons(m->m_pkthdr.len - sizeof(struct ip6_hdr));
	error = in6_embedscope(&ip6->ip6_src, src, NULL);
	if (error != 0)
		goto drop;
	error = in6_embedscope(&ip6->ip6_dst, dst, NULL);
	if (error != 0)
		goto drop;

	m->m_pkthdr.ph_rtableid = sc->sc_rdomain;

#if NPF > 0
	pf_pkt_addr_changed(m);
#endif
	etheripstat.etherip_opackets++;
	etheripstat.etherip_obytes += (m->m_pkthdr.len -
	    (sizeof(struct ip6_hdr) + sizeof(struct etherip_header)));

	return ip6_output(m, 0, NULL, IPV6_MINMTU, 0, NULL);

drop:
	m_freem(m);
	return (error);
}

int
ip6_etherip_input(struct mbuf **mp, int *offp, int proto)
{
	struct mbuf *m = *mp;
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	int off = *offp;
	struct etherip_softc *sc;
	const struct ip6_hdr *ip6;
	struct etherip_header *eip;
	struct sockaddr_in6 ipsrc, ipdst;
	struct sockaddr_in6 *src6, *dst6;
	struct ifnet *ifp = NULL;


	if (!etherip_allow) {
		m_freem(m);
		etheripstat.etherip_pdrops++;
		return IPPROTO_NONE;
	}

	ip6 = mtod(m, const struct ip6_hdr *);
	in6_recoverscope(&ipsrc, &ip6->ip6_src);
	in6_recoverscope(&ipdst, &ip6->ip6_dst);

	LIST_FOREACH(sc, &etherip_softc_list, sc_entry) {
		if (sc->sc_src.ss_family != AF_INET6 ||
		    sc->sc_dst.ss_family != AF_INET6)
			continue;

		src6 = (struct sockaddr_in6 *)&sc->sc_src;
		dst6 = (struct sockaddr_in6 *)&sc->sc_dst;

		if (IN6_ARE_ADDR_EQUAL(&src6->sin6_addr, &ipdst.sin6_addr) &&
		    src6->sin6_scope_id == ipdst.sin6_scope_id &&
		    IN6_ARE_ADDR_EQUAL(&dst6->sin6_addr, &ipsrc.sin6_addr) &&
		    dst6->sin6_scope_id == ipsrc.sin6_scope_id) {
			ifp = &sc->sc_ac.ac_if;
			break;
		}
	}

	if (ifp == NULL) {
#if NGIF > 0
		/*
		 * This path is nessesary for gif(4) and etherip(4) coexistence.
		 * This is tricky but the path will be removed soon when
		 * implementation of etherip is removed from gif(4).
		 */
		return etherip_input6(mp, offp, proto);
#else
		etheripstat.etherip_noifdrops++;
		m_freem(m);
		return IPPROTO_DONE;
#endif /* NGIF */
	}

	m_adj(m, off);
	m = m_pullup(m, sizeof(struct etherip_header));
	if (m == NULL) {
		etheripstat.etherip_adrops++;
		return IPPROTO_DONE;
	}

	eip = mtod(m, struct etherip_header *);
	if ((eip->eip_ver != ETHERIP_VERSION) || eip->eip_pad) {
		etheripstat.etherip_adrops++;
		m_freem(m);
		return IPPROTO_DONE;
	}
	etheripstat.etherip_ipackets++;
	etheripstat.etherip_ibytes += (m->m_pkthdr.len -
	    sizeof(struct etherip_header));

	m_adj(m, sizeof(struct etherip_header));
	m = m_pullup(m, sizeof(struct ether_header));
	if (m == NULL) {
		etheripstat.etherip_adrops++;
		return IPPROTO_DONE;
	}

	m->m_flags &= ~(M_BCAST|M_MCAST);

#if NPF > 0
	pf_pkt_addr_changed(m);
#endif

	ml_enqueue(&ml, m);
	if_input(ifp, &ml);

	return IPPROTO_DONE;
}

#endif /* INET6 */

int
ip_etherip_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp,
    void *newp, size_t newlen)
{
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return ENOTDIR;

	switch (name[0]) {
	case ETHERIPCTL_ALLOW:
		return sysctl_int(oldp, oldlenp, newp, newlen, &etherip_allow);
	case ETHERIPCTL_STATS:
		if (newp != NULL)
			return EPERM;
		return sysctl_struct(oldp, oldlenp, newp, newlen,
		    &etheripstat, sizeof(etheripstat));
	default:
		break;
	}

	return ENOPROTOOPT;
}
