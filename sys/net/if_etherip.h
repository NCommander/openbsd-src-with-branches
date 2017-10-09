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

#ifndef _NET_IF_ETHERIP_H_
#define _NET_IF_ETHERIP_H_

#if 0
/*
 * TODO:
 *   At this stage, struct etheripstat and struct etherip_header,
 *   and EtherIP sysctl objects are present at netinet/ip_ether.h.
 *   When implementation of etherip is removed from gif(4), there
 *   are moved here.
 */

extern int etherip_allow;
extern struct etheripstat etheripstat;

struct etheripstat {
	u_int64_t	etherips_hdrops;	/* packet shorter than header shows */
	u_int64_t	etherips_qfull;		/* bridge queue full, packet dropped */
	u_int64_t	etherips_noifdrops;	/* no interface/bridge information */
	u_int64_t	etherips_pdrops;	/* packet dropped due to policy */
	u_int64_t	etherips_adrops;	/* all other drops */
	u_int64_t	etherips_ipackets;	/* total input packets */
	u_int64_t	etherips_opackets;	/* total output packets */
	u_int64_t	etherips_ibytes;	/* input bytes */
	u_int64_t	etherips_obytes;	/* output bytes */
};

struct etherip_header {
#if BYTE_ORDER == LITTLE_ENDIAN
	unsigned int	eip_res:4;	/* reserved */
	unsigned int	eip_ver:4;	/* version */
#endif
#if BYTE_ORDER == BIG_ENDIAN
	unsigned int	eip_ver:4;	/* version */
	unsigned int	eip_res:4;	/* reserved */
#endif
	uint8_t	eip_pad;	/* required padding byte */
} __packed;

#define ETHERIP_VERSION		0x03

/*
 * Names for Ether-IP sysctl objects
 */
#define	ETHERIPCTL_ALLOW	1	/* accept incoming EtherIP packets */
#define	ETHERIPCTL_STATS	2	/* etherip stats */
#define	ETHERIPCTL_MAXID	3

#define ETHERIPCTL_NAMES {			\
		{ 0, 0 },			\
		{ "allow", CTLTYPE_INT },	\
		{ "stats", CTLTYPE_STRUCT },	\
}


#endif /* 0 */

int ip_etherip_sysctl(int *, uint, void *, size_t *, void *, size_t);
int ip_etherip_output(struct ifnet *, struct mbuf *);
int ip_etherip_input(struct mbuf **, int *, int, int);

#ifdef INET6
int ip6_etherip_output(struct ifnet *, struct mbuf *);
int ip6_etherip_input(struct mbuf **, int *, int, int);
#endif /* INET6 */


#endif /* _NET_IF_ETHERIP_H_ */
