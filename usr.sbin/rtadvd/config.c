/*	$OpenBSD: config.c,v 1.43 2015/01/16 06:40:20 deraadt Exp $	*/
/*	$KAME: config.c,v 1.62 2002/05/29 10:13:10 itojun Exp $	*/

/*
 * Copyright (C) 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/sysctl.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>

#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>

#include <arpa/inet.h>

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <stdint.h>

#include "rtadvd.h"
#include "advcap.h"
#include "timer.h"
#include "if.h"
#include "config.h"
#include "log.h"

static void makeentry(char *, size_t, int, char *);
static int getinet6sysctl(int);

extern struct ralist ralist;

void
getconfig(char *intface)
{
	int stat, i;
	char tbuf[BUFSIZ];
	struct rainfo *tmp;
	long val;
	int64_t val64;
	char buf[BUFSIZ];
	char *bp = buf;
	char *addr;
	static int forwarding = -1;

#define MUSTHAVE(var, cap)	\
    do {								\
	int64_t t;							\
	if ((t = agetnum(cap)) < 0) {					\
		log_warnx("need %s for interface %s",			\
			cap, intface);					\
		exit(1);						\
	}								\
	var = t;							\
     } while (0)
#define MAYHAVE(var, cap, def)	\
     do {								\
	if ((var = agetnum(cap)) < 0)					\
		var = def;						\
     } while (0)

	if ((stat = agetent(tbuf, intface)) <= 0) {
		memset(tbuf, 0, sizeof(tbuf));
		log_info("Could not parse configuration file for %s"
		    " or the configuration file doesn't exist."
		    " Treat it as default", intface);
	}

	if ((tmp = calloc(1, sizeof(*tmp))) == NULL)
		fatal("malloc");

	TAILQ_INIT(&tmp->prefixes);
	TAILQ_INIT(&tmp->rtinfos);
	TAILQ_INIT(&tmp->rdnsss);
	TAILQ_INIT(&tmp->dnssls);
	SLIST_INIT(&tmp->soliciters);

	/* check if we are allowed to forward packets (if not determined) */
	if (forwarding < 0) {
		if ((forwarding = getinet6sysctl(IPV6CTL_FORWARDING)) < 0)
			exit(1);
	}

	/* get interface information */
	if (agetflag("nolladdr"))
		tmp->advlinkopt = 0;
	else
		tmp->advlinkopt = 1;
	if (tmp->advlinkopt) {
		if ((tmp->sdl = if_nametosdl(intface)) == NULL) {
			log_warnx("can't get information of %s", intface);
			exit(1);
		}
		tmp->ifindex = tmp->sdl->sdl_index;
	} else
		tmp->ifindex = if_nametoindex(intface);
	strncpy(tmp->ifname, intface, sizeof(tmp->ifname));
	if ((tmp->phymtu = if_getmtu(intface)) == 0) {
		tmp->phymtu = IPV6_MMTU;
		log_warn("can't get interface mtu of %s. Treat as %d",
		    intface, IPV6_MMTU);
	}

	/*
	 * set router configuration variables.
	 */
	MAYHAVE(val, "maxinterval", DEF_MAXRTRADVINTERVAL);
	if (val < MIN_MAXINTERVAL || val > MAX_MAXINTERVAL) {
		log_warnx("maxinterval (%ld) on %s is invalid "
		    "(must be between %e and %u)", val,
		    intface, MIN_MAXINTERVAL, MAX_MAXINTERVAL);
		exit(1);
	}
	tmp->maxinterval = (u_int)val;
	MAYHAVE(val, "mininterval", tmp->maxinterval/3);
	if (val < MIN_MININTERVAL || val > (tmp->maxinterval * 3) / 4) {
		log_warnx("mininterval (%ld) on %s is invalid "
		    "(must be between %e and %d)",
		    val, intface, MIN_MININTERVAL, (tmp->maxinterval * 3) / 4);
		exit(1);
	}
	tmp->mininterval = (u_int)val;

	MAYHAVE(val, "chlim", DEF_ADVCURHOPLIMIT);
	tmp->hoplimit = val & 0xff;

	MAYHAVE(val, "raflags", 0);
	tmp->managedflg = val & ND_RA_FLAG_MANAGED;
	tmp->otherflg = val & ND_RA_FLAG_OTHER;
	tmp->rtpref = val & ND_RA_FLAG_RTPREF_MASK;
	if (tmp->rtpref == ND_RA_FLAG_RTPREF_RSV) {
		log_warnx("invalid router preference (%02x) on %s",
		    tmp->rtpref, intface);
		exit(1);
	}

	MAYHAVE(val, "rltime", tmp->maxinterval * 3);
	if (val && (val < tmp->maxinterval || val > MAXROUTERLIFETIME)) {
		log_warnx("router lifetime (%ld) on %s is invalid "
		    "(must be 0 or between %d and %d)",
		    val, intface,
		    tmp->maxinterval, MAXROUTERLIFETIME);
		exit(1);
	}
	/*
	 * Basically, hosts MUST NOT send Router Advertisement messages at any
	 * time (RFC 2461, Section 6.2.3). However, it would sometimes be
	 * useful to allow hosts to advertise some parameters such as prefix
	 * information and link MTU. Thus, we allow hosts to invoke rtadvd
	 * only when router lifetime (on every advertising interface) is
	 * explicitly set zero. (see also the above section)
	 */
	if (val && forwarding == 0) {
		log_warnx("non zero router lifetime is specified for %s, "
		    "which must not be allowed for hosts.  you must "
		    "change router lifetime or enable IPv6 forwarding.",
		    intface);
		exit(1);
	}
	tmp->lifetime = val & 0xffff;

	MAYHAVE(val, "rtime", DEF_ADVREACHABLETIME);
	if (val < 0 || val > MAXREACHABLETIME) {
		log_warnx("reachable time (%ld) on %s is invalid"
		    " (must be no greater than %d)",
		    val, intface, MAXREACHABLETIME);
		exit(1);
	}
	tmp->reachabletime = (u_int32_t)val;

	MAYHAVE(val64, "retrans", DEF_ADVRETRANSTIMER);
	if (val64 < 0 || val64 > 0xffffffff) {
		log_warnx("retrans time (%lld) on %s out of range",
		    (long long)val64, intface);
		exit(1);
	}
	tmp->retranstimer = (u_int32_t)val64;

	if (agetnum("hapref") != -1 || agetnum("hatime") != -1) {
		log_warnx("mobile-ip6 configuration not supported");
		exit(1);
	}
	/* prefix information */

	/*
	 * This is an implementation specific parameter to consider
	 * link propagation delays and poorly synchronized clocks when
	 * checking consistency of advertised lifetimes.
	 */
	MAYHAVE(val, "clockskew", 0);
	tmp->clockskew = val;

	tmp->pfxs = 0;
	for (i = -1; i < MAXPREFIX; i++) {
		struct prefix *pfx;
		char entbuf[256];

		makeentry(entbuf, sizeof(entbuf), i, "addr");
		addr = agetstr(entbuf, &bp);
		if (addr == NULL)
			continue;

		/* allocate memory to store prefix information */
		if ((pfx = calloc(1, sizeof(*pfx))) == NULL)
			fatal("calloc");

		/* link into chain */
		TAILQ_INSERT_TAIL(&tmp->prefixes, pfx, entry);
		tmp->pfxs++;

		pfx->origin = PREFIX_FROM_CONFIG;

		if (inet_pton(AF_INET6, addr, &pfx->prefix) != 1) {
			log_warn("inet_pton failed for %s", addr);
			exit(1);
		}
		if (IN6_IS_ADDR_MULTICAST(&pfx->prefix)) {
			log_warnx("multicast prefix (%s) must"
			    " not be advertised on %s",
			    addr, intface);
			exit(1);
		}
		if (IN6_IS_ADDR_LINKLOCAL(&pfx->prefix))
			log_info("link-local prefix (%s) will be"
			    " advertised on %s",
			    addr, intface);

		makeentry(entbuf, sizeof(entbuf), i, "prefixlen");
		MAYHAVE(val, entbuf, 64);
		if (val < 0 || val > 128) {
			log_warnx("prefixlen (%ld) for %s "
                            "on %s out of range",
			    val, addr, intface);
			exit(1);
		}
		pfx->prefixlen = (int)val;

		makeentry(entbuf, sizeof(entbuf), i, "pinfoflags");
		MAYHAVE(val, entbuf,
			(ND_OPT_PI_FLAG_ONLINK|ND_OPT_PI_FLAG_AUTO));
		pfx->onlinkflg = val & ND_OPT_PI_FLAG_ONLINK;
		pfx->autoconfflg = val & ND_OPT_PI_FLAG_AUTO;

		makeentry(entbuf, sizeof(entbuf), i, "vltime");
		MAYHAVE(val64, entbuf, DEF_ADVVALIDLIFETIME);
		if (val64 < 0 || val64 > 0xffffffff) {
			log_warnx("vltime (%lld) for"
			    " %s/%d on %s is out of range",
			    (long long)val64,
			    addr, pfx->prefixlen, intface);
			exit(1);
		}
		pfx->validlifetime = (u_int32_t)val64;

		makeentry(entbuf, sizeof(entbuf), i, "vltimedecr");
		if (agetflag(entbuf)) {
			struct timeval now;
			gettimeofday(&now, 0);
			pfx->vltimeexpire =
				now.tv_sec + pfx->validlifetime;
		}

		makeentry(entbuf, sizeof(entbuf), i, "pltime");
		MAYHAVE(val64, entbuf, DEF_ADVPREFERREDLIFETIME);
		if (val64 < 0 || val64 > 0xffffffff) {
			log_warnx("pltime (%lld) for %s/%d on %s"
			    " is out of range",
			    (long long)val64,
			    addr, pfx->prefixlen, intface);
			exit(1);
		}
		pfx->preflifetime = (u_int32_t)val64;

		makeentry(entbuf, sizeof(entbuf), i, "pltimedecr");
		if (agetflag(entbuf)) {
			struct timeval now;
			gettimeofday(&now, 0);
			pfx->pltimeexpire =
				now.tv_sec + pfx->preflifetime;
		}
	}
	if (tmp->pfxs == 0 && !agetflag("noifprefix"))
		get_prefix(tmp);

	tmp->rtinfocnt = 0;
	for (i = -1; i < MAXRTINFO; i++) {
		struct rtinfo *rti;
		char entbuf[256];
		const char *flagstr;

		makeentry(entbuf, sizeof(entbuf), i, "rtprefix");
		addr = agetstr(entbuf, &bp);
		if (addr == NULL)
			continue;

		rti = malloc(sizeof(struct rtinfo));
		if (rti == NULL)
			fatal("malloc");

		if (inet_pton(AF_INET6, addr, &rti->prefix) != 1) {
			log_warn("inet_pton failed for %s", addr);
			exit(1);
		}

		makeentry(entbuf, sizeof(entbuf), i, "rtplen");
		MAYHAVE(val, entbuf, 64);
		if (val < 0 || val > 128) {
			log_warnx("route prefixlen (%ld) for %s "
                            "on %s out of range",
			    val, addr, intface);
			exit(1);
		}
		rti->prefixlen = (int)val;

		makeentry(entbuf, sizeof(entbuf), i, "rtflags");
		if ((flagstr = agetstr(entbuf, &bp))) {
			val = 0;
			if (strchr(flagstr, 'h'))
				val |= ND_RA_FLAG_RTPREF_HIGH;
			if (strchr(flagstr, 'l')) {
				if (val & ND_RA_FLAG_RTPREF_HIGH) {
					log_warnx("the \'h\' and \'l\'"
					    " route preferences are"
					    " exclusive");
					exit(1);
				}
				val |= ND_RA_FLAG_RTPREF_LOW;
			}
		} else
			MAYHAVE(val, entbuf, 0);

		rti->rtpref = val & ND_RA_FLAG_RTPREF_MASK;
		if (rti->rtpref == ND_RA_FLAG_RTPREF_RSV) {
			log_warnx("invalid route preference (%02x)"
			    " for %s/%d on %s",
			    rti->rtpref, addr, rti->prefixlen, intface);
			exit(1);
		}

		makeentry(entbuf, sizeof(entbuf), i, "rtltime");
		MAYHAVE(val64, entbuf, -1);
		if (val64 == -1)
			val64 = tmp->lifetime;
		if (val64 < 0 || val64 >= 0xffffffff) {
			log_warnx("route lifetime (%d) "
			    " for %s/%d on %s out of range",
			    rti->rtpref, addr, rti->prefixlen, intface);
			exit(1);
		}
		rti->lifetime = (uint32_t)val64;

		TAILQ_INSERT_TAIL(&tmp->rtinfos, rti, entry);
		tmp->rtinfocnt++;
	}

	tmp->rdnsscnt = 0;
	for (i = -1; i < MAXRDNSS; ++i) {
		struct rdnss *rds;
		char entbuf[256];
		char *tmpaddr;

		makeentry(entbuf, sizeof(entbuf), i, "rdnss");
		addr = agetstr(entbuf, &bp);
		if (addr == NULL)
			continue;

		/* servers are separated by commas in the config file */
		val = 1;
		tmpaddr = addr;
		while (*tmpaddr++)
			if (*tmpaddr == ',')
				++val;

		rds = malloc(sizeof(struct rdnss) + val * sizeof(struct in6_addr));
		if (rds == NULL)
			fatal("malloc");

		TAILQ_INSERT_TAIL(&tmp->rdnsss, rds, entry);
		tmp->rdnsscnt++;

		rds->servercnt = val;

		makeentry(entbuf, sizeof(entbuf), i, "rdnssltime");
		MAYHAVE(val, entbuf, (tmp->maxinterval * 3) / 2);
		if (val < tmp->maxinterval || val > tmp->maxinterval * 2) {
			log_warnx("%s (%ld) on %s is invalid "
			    "(should be between %d and %d)",
			    entbuf, val, intface, tmp->maxinterval,
			    tmp->maxinterval * 2);
		}
		rds->lifetime = val;

		val = 0;
		while ((tmpaddr = strsep(&addr, ","))) {
			if (inet_pton(AF_INET6, tmpaddr, &rds->servers[val]) != 1) {
				log_warn("inet_pton failed for %s", tmpaddr);
				exit(1);
			}
			val++;
		}
	}

	tmp->dnsslcnt = 0;
	for (i = -1; i < MAXDNSSL; ++i) {
		struct dnssl *dsl;
		char entbuf[256];
		char *tmpsl;

		makeentry(entbuf, sizeof(entbuf), i, "dnssl");
		addr = agetstr(entbuf, &bp);
		if (addr == NULL)
			continue;

		dsl = malloc(sizeof(struct dnssl));
		if (dsl == NULL)
			fatal("malloc");

		TAILQ_INIT(&dsl->dnssldoms);

		while ((tmpsl = strsep(&addr, ","))) {
			struct dnssldom *dnsd;
			size_t len;

			len = strlen(tmpsl);

			/* if the domain is not "dot-terminated", add it */
			if (tmpsl[len - 1] != '.')
				len += 1;

			dnsd = malloc(sizeof(struct dnssldom) + len + 1);
			if (dnsd == NULL)
				fatal("malloc");

			dnsd->length = len;
			strlcpy(dnsd->domain, tmpsl, len + 1);
			dnsd->domain[len - 1] = '.';
			dnsd->domain[len] = '\0';

			TAILQ_INSERT_TAIL(&dsl->dnssldoms, dnsd, entry);
		}

		TAILQ_INSERT_TAIL(&tmp->dnssls, dsl, entry);
		tmp->dnsslcnt++;

		makeentry(entbuf, sizeof(entbuf), i, "dnsslltime");
		MAYHAVE(val, entbuf, (tmp->maxinterval * 3) / 2);
		if (val < tmp->maxinterval || val > tmp->maxinterval * 2) {
			log_warnx("%s (%ld) on %s is invalid "
			    "(should be between %d and %d)",
			    entbuf, val, intface, tmp->maxinterval,
			    tmp->maxinterval * 2);
		}
		dsl->lifetime = val;
	}

	MAYHAVE(val, "mtu", 0);
	if (val < 0 || val > 0xffffffff) {
		log_warnx("mtu (%ld) on %s out of range", val, intface);
		exit(1);
	}
	tmp->linkmtu = (u_int32_t)val;
	if (tmp->linkmtu == 0) {
		char *mtustr;

		if ((mtustr = agetstr("mtu", &bp)) &&
		    strcmp(mtustr, "auto") == 0)
			tmp->linkmtu = tmp->phymtu;
	}
	else if (tmp->linkmtu < IPV6_MMTU || tmp->linkmtu > tmp->phymtu) {
		log_warnx("advertised link mtu (%lu) on %s is invalid (must"
		    " be between least MTU (%d) and physical link MTU (%d)",
		    (unsigned long)tmp->linkmtu, intface,
		    IPV6_MMTU, tmp->phymtu);
		exit(1);
	}

	/* route information */
	MAYHAVE(val, "routes", -1);
	if (val != -1)
		log_info("route information option is not available");

	/* okey */
	SLIST_INSERT_HEAD(&ralist, tmp, entry);

	/* construct the sending packet */
	make_packet(tmp);

	/* set timer */
	tmp->timer = rtadvd_add_timer(ra_timeout, ra_timer_update,
				      tmp, tmp);
	ra_timer_update((void *)tmp, &tmp->timer->tm);
	rtadvd_set_timer(&tmp->timer->tm, tmp->timer);
}

void
get_prefix(struct rainfo *rai)
{
	struct ifaddrs *ifap, *ifa;
	struct prefix *pp;
	struct in6_addr *a;
	u_char *p, *ep, *m, *lim;
	u_char ntopbuf[INET6_ADDRSTRLEN];

	if (getifaddrs(&ifap) < 0)
		fatal("can't get interface addresses");

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		int plen;

		if (strcmp(ifa->ifa_name, rai->ifname) != 0)
			continue;
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		a = &((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
		if (IN6_IS_ADDR_LINKLOCAL(a))
			continue;
		/* get prefix length */
		m = (u_char *)&((struct sockaddr_in6 *)ifa->ifa_netmask)->sin6_addr;
		lim = (u_char *)(ifa->ifa_netmask) + ifa->ifa_netmask->sa_len;
		plen = prefixlen(m, lim);
		if (plen <= 0 || plen > 128)
			fatalx("failed to get prefixlen or prefix is invalid");
		if (plen == 128)	/* XXX */
			continue;
		if (find_prefix(rai, a, plen)) {
			/* ignore a duplicated prefix. */
			continue;
		}

		/* allocate memory to store prefix info. */
		if ((pp = calloc(1, sizeof(*pp))) == NULL)
			fatal("calloc");

		/* set prefix, sweep bits outside of prefixlen */
		pp->prefixlen = plen;
		memcpy(&pp->prefix, a, sizeof(*a));
		if (1)
		{
			p = (u_char *)&pp->prefix;
			ep = (u_char *)(&pp->prefix + 1);
			while (m < lim && p < ep)
				*p++ &= *m++;
			while (p < ep)
				*p++ = 0x00;
		}
	        if (!inet_ntop(AF_INET6, &pp->prefix, ntopbuf,
	            sizeof(ntopbuf)))
			fatal("inet_ntop failed");
		log_debug("add %s/%d to prefix list on %s",
		    ntopbuf, pp->prefixlen, rai->ifname);

		/* set other fields with protocol defaults */
		pp->validlifetime = DEF_ADVVALIDLIFETIME;
		pp->preflifetime = DEF_ADVPREFERREDLIFETIME;
		pp->onlinkflg = 1;
		pp->autoconfflg = 1;
		pp->origin = PREFIX_FROM_KERNEL;

		/* link into chain */
		TAILQ_INSERT_TAIL(&rai->prefixes, pp, entry);

		/* counter increment */
		rai->pfxs++;
	}

	freeifaddrs(ifap);
}

static void
makeentry(char *buf, size_t len, int id, char *string)
{

	if (id < 0)
		strlcpy(buf, string, len);
	else
		snprintf(buf, len, "%s%d", string, id);
}

/*
 * Add a prefix to the list of specified interface and reconstruct
 * the outgoing packet.
 * The prefix must not be in the list.
 * XXX: other parameter of the prefix(e.g. lifetime) shoule be
 * able to be specified.
 */
static void
add_prefix(struct rainfo *rai, struct in6_prefixreq *ipr)
{
	struct prefix *prefix;
	u_char ntopbuf[INET6_ADDRSTRLEN];

	if ((prefix = calloc(1, sizeof(*prefix))) == NULL) {
		log_warn("calloc");
		return;		/* XXX: error or exit? */
	}
	prefix->prefix = ipr->ipr_prefix.sin6_addr;
	prefix->prefixlen = ipr->ipr_plen;
	prefix->validlifetime = ipr->ipr_vltime;
	prefix->preflifetime = ipr->ipr_pltime;
	prefix->onlinkflg = ipr->ipr_raf_onlink;
	prefix->autoconfflg = ipr->ipr_raf_auto;
	prefix->origin = PREFIX_FROM_DYNAMIC;

	TAILQ_INSERT_TAIL(&rai->prefixes, prefix, entry);

	log_debug("new prefix %s/%d was added on %s",
	    inet_ntop(AF_INET6, &ipr->ipr_prefix.sin6_addr,
	       ntopbuf, INET6_ADDRSTRLEN),
	    ipr->ipr_plen, rai->ifname);

	/* free the previous packet */
	free(rai->ra_data);
	rai->ra_data = NULL;

	/* reconstruct the packet */
	rai->pfxs++;
	make_packet(rai);

	/*
	 * reset the timer so that the new prefix will be advertised quickly.
	 */
	rai->initcounter = 0;
	ra_timer_update((void *)rai, &rai->timer->tm);
	rtadvd_set_timer(&rai->timer->tm, rai->timer);
}

/*
 * Delete a prefix to the list of specified interface and reconstruct
 * the outgoing packet.
 * The prefix must be in the list.
 */
void
delete_prefix(struct rainfo *rai, struct prefix *prefix)
{
	u_char ntopbuf[INET6_ADDRSTRLEN];

	TAILQ_REMOVE(&rai->prefixes, prefix, entry);
	log_debug("prefix %s/%d was deleted on %s",
	    inet_ntop(AF_INET6, &prefix->prefix, ntopbuf, INET6_ADDRSTRLEN), 
	    prefix->prefixlen, rai->ifname);
	free(prefix);
	rai->pfxs--;
	make_packet(rai);
}

/*
 * Try to get an in6_prefixreq contents for a prefix which matches
 * ipr->ipr_prefix and ipr->ipr_plen and belongs to
 * the interface whose name is ipr->ipr_name[].
 */
static int
init_prefix(struct in6_prefixreq *ipr)
{
#if 0
	int s;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		log_warn("socket");
		exit(1);
	}

	if (ioctl(s, SIOCGIFPREFIX_IN6, (caddr_t)ipr) < 0) {
		log_warn("ioctl:SIOCGIFFLAGS: failed for %s", ifr.ifr_name);

		ipr->ipr_vltime = DEF_ADVVALIDLIFETIME;
		ipr->ipr_pltime = DEF_ADVPREFERREDLIFETIME;
		ipr->ipr_raf_onlink = 1;
		ipr->ipr_raf_auto = 1;
		/* omit other field initialization */
	}
	else if (ipr->ipr_origin < PR_ORIG_RR) {
		u_char ntopbuf[INET6_ADDRSTRLEN];

		log_warn("Added prefix(%s)'s origin %d is"
		    " lower than PR_ORIG_RR(router renumbering)."
		    " This should not happen if I am router",
		    inet_ntop(AF_INET6, &ipr->ipr_prefix.sin6_addr, ntopbuf,
			sizeof(ntopbuf)), ipr->ipr_origin);
		close(s);
		return 1;
	}

	close(s);
	return 0;
#else
	ipr->ipr_vltime = DEF_ADVVALIDLIFETIME;
	ipr->ipr_pltime = DEF_ADVPREFERREDLIFETIME;
	ipr->ipr_raf_onlink = 1;
	ipr->ipr_raf_auto = 1;
	return 0;
#endif
}

void
make_prefix(struct rainfo *rai, int ifindex, struct in6_addr *addr, int plen)
{
	struct in6_prefixreq ipr;

	memset(&ipr, 0, sizeof(ipr));
	if (if_indextoname(ifindex, ipr.ipr_name) == NULL) {
		log_warn("Prefix added interface No.%d doesn't"
		    " exist. This should not happen!", ifindex);
		exit(1);
	}
	ipr.ipr_prefix.sin6_len = sizeof(ipr.ipr_prefix);
	ipr.ipr_prefix.sin6_family = AF_INET6;
	ipr.ipr_prefix.sin6_addr = *addr;
	ipr.ipr_plen = plen;

	if (init_prefix(&ipr))
		return; /* init failed by some error */
	add_prefix(rai, &ipr);
}

void
make_packet(struct rainfo *rainfo)
{
	size_t packlen, lladdroptlen = 0;
	char *buf;
	struct nd_router_advert *ra;
	struct nd_opt_prefix_info *ndopt_pi;
	struct nd_opt_mtu *ndopt_mtu;
	struct nd_opt_route_info *ndopt_rti;
	struct nd_opt_rdnss *ndopt_rdnss;
	struct nd_opt_dnssl *ndopt_dnssl;
	struct prefix *pfx;
	struct rtinfo *rti;
	struct rdnss *rds;
	struct dnssl *dsl;
	struct dnssldom *dnsd;

	/* calculate total length */
	packlen = sizeof(struct nd_router_advert);
	if (rainfo->advlinkopt) {
		if ((lladdroptlen = lladdropt_length(rainfo->sdl)) == 0) {
			log_info("link-layer address option has"
			    " null length on %s.  Treat as not included.",
			    rainfo->ifname);
			rainfo->advlinkopt = 0;
		}
		packlen += lladdroptlen;
	}
	if (rainfo->pfxs)
		packlen += sizeof(struct nd_opt_prefix_info) * rainfo->pfxs;
	if (rainfo->linkmtu)
		packlen += sizeof(struct nd_opt_mtu);
	TAILQ_FOREACH(rti, &rainfo->rtinfos, entry)
		packlen += sizeof(struct nd_opt_route_info) +
		    ((rti->prefixlen + 0x3f) >> 6) * 8;
	TAILQ_FOREACH(rds, &rainfo->rdnsss, entry)
		packlen += sizeof(struct nd_opt_rdnss) + 16 * rds->servercnt;
	TAILQ_FOREACH(dsl, &rainfo->dnssls, entry) {
		size_t domains_size = 0;

		packlen += sizeof(struct nd_opt_dnssl);

		/*
		 * Each domain in the packet ends with a null byte. Account for
		 * that here.
		 */
		TAILQ_FOREACH(dnsd, &dsl->dnssldoms, entry)
			domains_size += dnsd->length + 1;

		domains_size = (domains_size + 7) & ~7;

		packlen += domains_size;
	}

	/* allocate memory for the packet */
	if ((buf = malloc(packlen)) == NULL)
		fatal("malloc");
	if (rainfo->ra_data) {
		/* free the previous packet */
		free(rainfo->ra_data);
		rainfo->ra_data = NULL;
	}
	rainfo->ra_data = buf;
	/* XXX: what if packlen > 576? */
	rainfo->ra_datalen = packlen;

	/*
	 * construct the packet
	 */
	ra = (struct nd_router_advert *)buf;
	ra->nd_ra_type = ND_ROUTER_ADVERT;
	ra->nd_ra_code = 0;
	ra->nd_ra_cksum = 0;
	ra->nd_ra_curhoplimit = (u_int8_t)(0xff & rainfo->hoplimit);
	ra->nd_ra_flags_reserved = 0; /* just in case */
	/*
	 * XXX: the router preference field, which is a 2-bit field, should be
	 * initialized before other fields.
	 */
	ra->nd_ra_flags_reserved = 0xff & rainfo->rtpref;
	ra->nd_ra_flags_reserved |=
		rainfo->managedflg ? ND_RA_FLAG_MANAGED : 0;
	ra->nd_ra_flags_reserved |=
		rainfo->otherflg ? ND_RA_FLAG_OTHER : 0;
	ra->nd_ra_router_lifetime = htons(rainfo->lifetime);
	ra->nd_ra_reachable = htonl(rainfo->reachabletime);
	ra->nd_ra_retransmit = htonl(rainfo->retranstimer);
	buf += sizeof(*ra);

	if (rainfo->advlinkopt) {
		lladdropt_fill(rainfo->sdl, (struct nd_opt_hdr *)buf);
		buf += lladdroptlen;
	}

	if (rainfo->linkmtu) {
		ndopt_mtu = (struct nd_opt_mtu *)buf;
		ndopt_mtu->nd_opt_mtu_type = ND_OPT_MTU;
		ndopt_mtu->nd_opt_mtu_len = 1;
		ndopt_mtu->nd_opt_mtu_reserved = 0;
		ndopt_mtu->nd_opt_mtu_mtu = htonl(rainfo->linkmtu);
		buf += sizeof(struct nd_opt_mtu);
	}

	TAILQ_FOREACH(pfx, &rainfo->prefixes, entry) {
		u_int32_t vltime, pltime;
		struct timeval now;

		ndopt_pi = (struct nd_opt_prefix_info *)buf;
		ndopt_pi->nd_opt_pi_type = ND_OPT_PREFIX_INFORMATION;
		ndopt_pi->nd_opt_pi_len = 4;
		ndopt_pi->nd_opt_pi_prefix_len = pfx->prefixlen;
		ndopt_pi->nd_opt_pi_flags_reserved = 0;
		if (pfx->onlinkflg)
			ndopt_pi->nd_opt_pi_flags_reserved |=
				ND_OPT_PI_FLAG_ONLINK;
		if (pfx->autoconfflg)
			ndopt_pi->nd_opt_pi_flags_reserved |=
				ND_OPT_PI_FLAG_AUTO;
		if (pfx->vltimeexpire || pfx->pltimeexpire)
			gettimeofday(&now, NULL);
		if (pfx->vltimeexpire == 0)
			vltime = pfx->validlifetime;
		else
			vltime = (u_int32_t)(pfx->vltimeexpire > now.tv_sec ?
				pfx->vltimeexpire - now.tv_sec : 0);
		if (pfx->pltimeexpire == 0)
			pltime = pfx->preflifetime;
		else
			pltime = (u_int32_t)(pfx->pltimeexpire > now.tv_sec ? 
				pfx->pltimeexpire - now.tv_sec : 0);
		if (vltime < pltime) {
			/*
			 * this can happen if vltime is decrement but pltime
			 * is not.
			 */
			pltime = vltime;
		}
		ndopt_pi->nd_opt_pi_valid_time = htonl(vltime);
		ndopt_pi->nd_opt_pi_preferred_time = htonl(pltime);
		ndopt_pi->nd_opt_pi_reserved2 = 0;
		ndopt_pi->nd_opt_pi_prefix = pfx->prefix;

		buf += sizeof(struct nd_opt_prefix_info);
	}

	TAILQ_FOREACH(rti, &rainfo->rtinfos, entry) {
		uint8_t psize = (rti->prefixlen + 0x3f) >> 6;

		ndopt_rti = (struct nd_opt_route_info *)buf;
		ndopt_rti->nd_opt_rti_type = ND_OPT_ROUTE_INFO;
		ndopt_rti->nd_opt_rti_len = 1 + psize;
		ndopt_rti->nd_opt_rti_prefixlen = rti->prefixlen;
		ndopt_rti->nd_opt_rti_flags = 0xff & rti->rtpref;
		ndopt_rti->nd_opt_rti_lifetime = htonl(rti->lifetime);
		memcpy(ndopt_rti + 1, &rti->prefix, psize * 8);
		buf += sizeof(struct nd_opt_route_info) + psize * 8;
	}

	TAILQ_FOREACH(rds, &rainfo->rdnsss, entry) {
		ndopt_rdnss = (struct nd_opt_rdnss *)buf;
		ndopt_rdnss->nd_opt_rdnss_type = ND_OPT_RDNSS;
		/*
		 * An IPv6 address is 16 bytes, so multiply the number of
		 * addresses by two to get a size in units of 8 bytes.
		 */
		ndopt_rdnss->nd_opt_rdnss_len = 1 + rds->servercnt * 2;
		ndopt_rdnss->nd_opt_rdnss_reserved = 0;
		ndopt_rdnss->nd_opt_rdnss_lifetime = htonl(rds->lifetime);

		buf += sizeof(struct nd_opt_rdnss);

		memcpy(buf, rds->servers, rds->servercnt * 16);
		buf += rds->servercnt * 16;
	}

	TAILQ_FOREACH(dsl, &rainfo->dnssls, entry) {
		u_int32_t size;

		ndopt_dnssl = (struct nd_opt_dnssl *)buf;
		ndopt_dnssl->nd_opt_dnssl_type = ND_OPT_DNSSL;
		ndopt_dnssl->nd_opt_dnssl_reserved = 0;
		ndopt_dnssl->nd_opt_dnssl_lifetime = htonl(dsl->lifetime);

		size = 0;
		TAILQ_FOREACH(dnsd, &dsl->dnssldoms, entry)
			size += dnsd->length + 1;
		/* align size on the next 8 byte boundary */
		size = (size + 7) & ~7;
		ndopt_dnssl->nd_opt_dnssl_len = 1 + size / 8;

		buf += sizeof(struct nd_opt_dnssl);

		TAILQ_FOREACH(dnsd, &dsl->dnssldoms, entry) {
			char *curlabel_begin;
			char *curlabel_end;

			curlabel_begin = dnsd->domain;
			while ((curlabel_end = strchr(curlabel_begin, '.'))
			    != NULL && curlabel_end > curlabel_begin)
			{
				size_t curlabel_size;

				curlabel_size = curlabel_end - curlabel_begin;
				*buf++ = curlabel_size;
				memcpy(buf, curlabel_begin, curlabel_size);
				buf += curlabel_size;
				curlabel_begin = curlabel_end + 1;
			}

			/* null-terminate the current domain */
			*buf++ = '\0';
		}

		/* zero out the end of the current option */
		while (((uintptr_t)buf) % 8 != 0)
			*buf++ = '\0';
	}

	return;
}

static int
getinet6sysctl(int code)
{
	int mib[] = { CTL_NET, PF_INET6, IPPROTO_IPV6, 0 };
	int value;
	size_t size;

	mib[3] = code;
	size = sizeof(value);
	if (sysctl(mib, sizeof(mib)/sizeof(mib[0]), &value, &size, NULL, 0)
	    < 0) {
		log_warn("failed to get ip6 sysctl(%d)", code);
		return(-1);
	}
	else
		return(value);
}
