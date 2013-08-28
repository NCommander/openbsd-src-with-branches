/*	$OpenBSD: pfctl.c,v 1.314 2012/09/19 15:52:17 camield Exp $ */

/*
 * Copyright (c) 2001 Daniel Hartmeier
 * Copyright (c) 2002,2003 Henning Brauer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <net/if.h>
#include <netinet/in.h>
#include <net/pfvar.h>
#include <arpa/inet.h>
#include <altq/altq.h>
#include <sys/sysctl.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <syslog.h>

#include "pfctl_parser.h"
#include "pfctl.h"

void	 usage(void);
int	 pfctl_enable(int, int);
int	 pfctl_disable(int, int);
int	 pfctl_clear_stats(int, const char *, int);
int	 pfctl_clear_interface_flags(int, int);
int	 pfctl_clear_rules(int, int, char *);
int	 pfctl_clear_altq(int, int);
int	 pfctl_clear_src_nodes(int, int);
int	 pfctl_clear_states(int, const char *, int);
void	 pfctl_addrprefix(char *, struct pf_addr *);
int	 pfctl_kill_src_nodes(int, const char *, int);
int	 pfctl_net_kill_states(int, const char *, int);
int	 pfctl_label_kill_states(int, const char *, int);
int	 pfctl_id_kill_states(int, const char *, int);
void	 pfctl_init_options(struct pfctl *);
int	 pfctl_load_options(struct pfctl *);
int	 pfctl_load_limit(struct pfctl *, unsigned int, unsigned int);
int	 pfctl_load_timeout(struct pfctl *, unsigned int, unsigned int);
int	 pfctl_load_debug(struct pfctl *, unsigned int);
int	 pfctl_load_logif(struct pfctl *, char *);
int	 pfctl_load_hostid(struct pfctl *, unsigned int);
int	 pfctl_load_reassembly(struct pfctl *, u_int32_t);
void	 pfctl_print_rule_counters(struct pf_rule *, int);
int	 pfctl_show_rules(int, char *, int, enum pfctl_show, char *, int, int,
	    long);
int	 pfctl_show_src_nodes(int, int);
int	 pfctl_show_states(int, const char *, int);
int	 pfctl_show_status(int, int);
int	 pfctl_show_timeouts(int, int);
int	 pfctl_show_limits(int, int);
void	 pfctl_debug(int, u_int32_t, int);
int	 pfctl_test_altqsupport(int, int);
int	 pfctl_show_anchors(int, int, char *);
int	 pfctl_ruleset_trans(struct pfctl *, char *, struct pf_anchor *);
int	 pfctl_load_ruleset(struct pfctl *, char *, struct pf_ruleset *, int);
int	 pfctl_load_rule(struct pfctl *, char *, struct pf_rule *, int);
const char	*pfctl_lookup_option(char *, const char **);
void	pfctl_state_store(int, const char *);
void	pfctl_state_load(int, const char *);

struct pf_anchor_global	 pf_anchors;
struct pf_anchor	 pf_main_anchor;

const char	*clearopt;
char		*rulesopt;
const char	*showopt;
const char	*debugopt;
char		*anchoropt;
const char	*optiopt = NULL;
char		*pf_device = "/dev/pf";
char		*ifaceopt;
char		*tableopt;
const char	*tblcmdopt;
int		 src_node_killers;
char		*src_node_kill[2];
int		 state_killers;
char		*state_kill[2];
int		 loadaltq = 1;
int		 altqsupport;

int		 dev = -1;
int		 first_title = 1;
int		 labels = 0;

#define INDENT(d, o)	do {						\
				if (o) {				\
					int i;				\
					for (i=0; i < d; i++)		\
						printf("  ");		\
				}					\
			} while (0)					\


static const struct {
	const char	*name;
	int		index;
} pf_limits[] = {
	{ "states",		PF_LIMIT_STATES },
	{ "src-nodes",		PF_LIMIT_SRC_NODES },
	{ "frags",		PF_LIMIT_FRAGS },
	{ "tables",		PF_LIMIT_TABLES },
	{ "table-entries",	PF_LIMIT_TABLE_ENTRIES },
	{ NULL,			0 }
};

struct pf_hint {
	const char	*name;
	int		timeout;
};
static const struct pf_hint pf_hint_normal[] = {
	{ "tcp.first",		2 * 60 },
	{ "tcp.opening",	30 },
	{ "tcp.established",	24 * 60 * 60 },
	{ "tcp.closing",	15 * 60 },
	{ "tcp.finwait",	45 },
	{ "tcp.closed",		90 },
	{ "tcp.tsdiff",		30 },
	{ NULL,			0 }
};
static const struct pf_hint pf_hint_satellite[] = {
	{ "tcp.first",		3 * 60 },
	{ "tcp.opening",	30 + 5 },
	{ "tcp.established",	24 * 60 * 60 },
	{ "tcp.closing",	15 * 60 + 5 },
	{ "tcp.finwait",	45 + 5 },
	{ "tcp.closed",		90 + 5 },
	{ "tcp.tsdiff",		60 },
	{ NULL,			0 }
};
static const struct pf_hint pf_hint_conservative[] = {
	{ "tcp.first",		60 * 60 },
	{ "tcp.opening",	15 * 60 },
	{ "tcp.established",	5 * 24 * 60 * 60 },
	{ "tcp.closing",	60 * 60 },
	{ "tcp.finwait",	10 * 60 },
	{ "tcp.closed",		3 * 60 },
	{ "tcp.tsdiff",		60 },
	{ NULL,			0 }
};
static const struct pf_hint pf_hint_aggressive[] = {
	{ "tcp.first",		30 },
	{ "tcp.opening",	5 },
	{ "tcp.established",	5 * 60 * 60 },
	{ "tcp.closing",	60 },
	{ "tcp.finwait",	30 },
	{ "tcp.closed",		30 },
	{ "tcp.tsdiff",		10 },
	{ NULL,			0 }
};

static const struct {
	const char *name;
	const struct pf_hint *hint;
} pf_hints[] = {
	{ "normal",		pf_hint_normal },
	{ "satellite",		pf_hint_satellite },
	{ "high-latency",	pf_hint_satellite },
	{ "conservative",	pf_hint_conservative },
	{ "aggressive",		pf_hint_aggressive },
	{ NULL,			NULL }
};

static const char *clearopt_list[] = {
	"queue", "rules", "Sources",
	"states", "info", "Tables", "osfp", "all", NULL
};

static const char *showopt_list[] = {
	"queue", "rules", "Anchors", "Sources", "states", "info",
	"Interfaces", "labels", "timeouts", "memory", "Tables", "osfp",
	"all", NULL
};

static const char *tblcmdopt_list[] = {
	"kill", "flush", "add", "delete", "replace", "show",
	"test", "zero", "expire", NULL
};

static const char *debugopt_list[] = {
	"debug", "info", "notice", "warning",
	"error", "crit", "alert", "emerg",
	"none", "urgent", "misc", "loud",
	NULL
};

static const char *optiopt_list[] = {
	"none", "basic", "profile", NULL
};

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-deghnPqrvz] ", __progname);
	fprintf(stderr, "[-a anchor] [-D macro=value] [-F modifier]\n");
	fprintf(stderr, "\t[-f file] [-i interface] [-K host | network]\n");
	fprintf(stderr, "\t[-k host | network | label | id] ");
	fprintf(stderr, "[-L statefile] [-o level] [-p device]\n");
	fprintf(stderr, "\t[-S statefile] [-s modifier [-R id]] ");
	fprintf(stderr, "[-t table -T command [address ...]]\n");
	fprintf(stderr, "\t[-x level]\n");
	exit(1);
}

int
pfctl_enable(int dev, int opts)
{
	if (ioctl(dev, DIOCSTART)) {
		if (errno == EEXIST)
			errx(1, "pf already enabled");
		else
			err(1, "DIOCSTART");
	}
	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "pf enabled\n");

	if (altqsupport && ioctl(dev, DIOCSTARTALTQ))
		if (errno != EEXIST)
			err(1, "DIOCSTARTALTQ");

	return (0);
}

int
pfctl_disable(int dev, int opts)
{
	if (ioctl(dev, DIOCSTOP)) {
		if (errno == ENOENT)
			errx(1, "pf not enabled");
		else
			err(1, "DIOCSTOP");
	}
	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "pf disabled\n");

	if (altqsupport && ioctl(dev, DIOCSTOPALTQ))
			if (errno != ENOENT)
				err(1, "DIOCSTOPALTQ");

	return (0);
}

int
pfctl_clear_stats(int dev, const char *iface, int opts)
{
	struct pfioc_iface pi;

	memset(&pi, 0, sizeof(pi));
	if (iface != NULL && strlcpy(pi.pfiio_name, iface,
	    sizeof(pi.pfiio_name)) >= sizeof(pi.pfiio_name))
		errx(1, "invalid interface: %s", iface);

	if (ioctl(dev, DIOCCLRSTATUS, &pi))
		err(1, "DIOCCLRSTATUS");
	if ((opts & PF_OPT_QUIET) == 0) {
		fprintf(stderr, "pf: statistics cleared");
		if (iface != NULL)
			fprintf(stderr, " for interface %s", iface);
		fprintf(stderr, "\n");
	}
	return (0);
}

int
pfctl_clear_interface_flags(int dev, int opts)
{
	struct pfioc_iface	pi;

	if ((opts & PF_OPT_NOACTION) == 0) {
		bzero(&pi, sizeof(pi));
		pi.pfiio_flags = PFI_IFLAG_SKIP;

		if (ioctl(dev, DIOCCLRIFFLAG, &pi))
			err(1, "DIOCCLRIFFLAG");
		if ((opts & PF_OPT_QUIET) == 0)
			fprintf(stderr, "pf: interface flags reset\n");
	}
	return (0);
}

int
pfctl_clear_rules(int dev, int opts, char *anchorname)
{
	struct pfr_buffer t;

	memset(&t, 0, sizeof(t));
	t.pfrb_type = PFRB_TRANS;
	if (pfctl_add_trans(&t, PF_TRANS_RULESET, anchorname) ||
	    pfctl_trans(dev, &t, DIOCXBEGIN, 0) ||
	    pfctl_trans(dev, &t, DIOCXCOMMIT, 0))
		err(1, "pfctl_clear_rules");
	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "rules cleared\n");
	return (0);
}

int
pfctl_clear_altq(int dev, int opts)
{
	struct pfr_buffer t;

	if (!altqsupport)
		return (-1);
	memset(&t, 0, sizeof(t));
	t.pfrb_type = PFRB_TRANS;
	if (pfctl_add_trans(&t, PF_TRANS_ALTQ, "") ||
	    pfctl_trans(dev, &t, DIOCXBEGIN, 0) ||
	    pfctl_trans(dev, &t, DIOCXCOMMIT, 0))
		err(1, "pfctl_clear_altq");
	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "altq cleared\n");
	return (0);
}

int
pfctl_clear_src_nodes(int dev, int opts)
{
	if (ioctl(dev, DIOCCLRSRCNODES))
		err(1, "DIOCCLRSRCNODES");
	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "source tracking entries cleared\n");
	return (0);
}

int
pfctl_clear_states(int dev, const char *iface, int opts)
{
	struct pfioc_state_kill psk;

	memset(&psk, 0, sizeof(psk));
	if (iface != NULL && strlcpy(psk.psk_ifname, iface,
	    sizeof(psk.psk_ifname)) >= sizeof(psk.psk_ifname))
		errx(1, "invalid interface: %s", iface);

	if (ioctl(dev, DIOCCLRSTATES, &psk))
		err(1, "DIOCCLRSTATES");
	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "%d states cleared\n", psk.psk_killed);
	return (0);
}

void
pfctl_addrprefix(char *addr, struct pf_addr *mask)
{
	char *p;
	const char *errstr;
	int prefix, ret_ga, q, r;
	struct addrinfo hints, *res;

	if ((p = strchr(addr, '/')) == NULL)
		return;

	*p++ = '\0';
	prefix = strtonum(p, 0, 128, &errstr);
	if (errstr)
		errx(1, "prefix is %s: %s", errstr, p);

	bzero(&hints, sizeof(hints));
	/* prefix only with numeric addresses */
	hints.ai_flags |= AI_NUMERICHOST;

	if ((ret_ga = getaddrinfo(addr, NULL, &hints, &res))) {
		errx(1, "getaddrinfo: %s", gai_strerror(ret_ga));
		/* NOTREACHED */
	}

	if (res->ai_family == AF_INET && prefix > 32)
		errx(1, "prefix too long for AF_INET");
	else if (res->ai_family == AF_INET6 && prefix > 128)
		errx(1, "prefix too long for AF_INET6");

	q = prefix >> 3;
	r = prefix & 7;
	switch (res->ai_family) {
	case AF_INET:
		bzero(&mask->v4, sizeof(mask->v4));
		mask->v4.s_addr = htonl((u_int32_t)
		    (0xffffffffffULL << (32 - prefix)));
		break;
	case AF_INET6:
		bzero(&mask->v6, sizeof(mask->v6));
		if (q > 0)
			memset((void *)&mask->v6, 0xff, q);
		if (r > 0)
			*((u_char *)&mask->v6 + q) =
			    (0xff00 >> r) & 0xff;
		break;
	}
	freeaddrinfo(res);
}

int
pfctl_kill_src_nodes(int dev, const char *iface, int opts)
{
	struct pfioc_src_node_kill psnk;
	struct addrinfo *res[2], *resp[2];
	struct sockaddr last_src, last_dst;
	int killed, sources, dests;
	int ret_ga;

	killed = sources = dests = 0;

	memset(&psnk, 0, sizeof(psnk));
	memset(&psnk.psnk_src.addr.v.a.mask, 0xff,
	    sizeof(psnk.psnk_src.addr.v.a.mask));
	memset(&last_src, 0xff, sizeof(last_src));
	memset(&last_dst, 0xff, sizeof(last_dst));

	pfctl_addrprefix(src_node_kill[0], &psnk.psnk_src.addr.v.a.mask);

	if ((ret_ga = getaddrinfo(src_node_kill[0], NULL, NULL, &res[0]))) {
		errx(1, "getaddrinfo: %s", gai_strerror(ret_ga));
		/* NOTREACHED */
	}
	for (resp[0] = res[0]; resp[0]; resp[0] = resp[0]->ai_next) {
		if (resp[0]->ai_addr == NULL)
			continue;
		/* We get lots of duplicates.  Catch the easy ones */
		if (memcmp(&last_src, resp[0]->ai_addr, sizeof(last_src)) == 0)
			continue;
		last_src = *(struct sockaddr *)resp[0]->ai_addr;

		psnk.psnk_af = resp[0]->ai_family;
		sources++;

		if (psnk.psnk_af == AF_INET)
			psnk.psnk_src.addr.v.a.addr.v4 =
			    ((struct sockaddr_in *)resp[0]->ai_addr)->sin_addr;
		else if (psnk.psnk_af == AF_INET6)
			psnk.psnk_src.addr.v.a.addr.v6 =
			    ((struct sockaddr_in6 *)resp[0]->ai_addr)->
			    sin6_addr;
		else
			errx(1, "Unknown address family %d", psnk.psnk_af);

		if (src_node_killers > 1) {
			dests = 0;
			memset(&psnk.psnk_dst.addr.v.a.mask, 0xff,
			    sizeof(psnk.psnk_dst.addr.v.a.mask));
			memset(&last_dst, 0xff, sizeof(last_dst));
			pfctl_addrprefix(src_node_kill[1],
			    &psnk.psnk_dst.addr.v.a.mask);
			if ((ret_ga = getaddrinfo(src_node_kill[1], NULL, NULL,
			    &res[1]))) {
				errx(1, "getaddrinfo: %s",
				    gai_strerror(ret_ga));
				/* NOTREACHED */
			}
			for (resp[1] = res[1]; resp[1];
			    resp[1] = resp[1]->ai_next) {
				if (resp[1]->ai_addr == NULL)
					continue;
				if (psnk.psnk_af != resp[1]->ai_family)
					continue;

				if (memcmp(&last_dst, resp[1]->ai_addr,
				    sizeof(last_dst)) == 0)
					continue;
				last_dst = *(struct sockaddr *)resp[1]->ai_addr;

				dests++;

				if (psnk.psnk_af == AF_INET)
					psnk.psnk_dst.addr.v.a.addr.v4 =
					    ((struct sockaddr_in *)resp[1]->
					    ai_addr)->sin_addr;
				else if (psnk.psnk_af == AF_INET6)
					psnk.psnk_dst.addr.v.a.addr.v6 =
					    ((struct sockaddr_in6 *)resp[1]->
					    ai_addr)->sin6_addr;
				else
					errx(1, "Unknown address family %d",
					    psnk.psnk_af);

				if (ioctl(dev, DIOCKILLSRCNODES, &psnk))
					err(1, "DIOCKILLSRCNODES");
				killed += psnk.psnk_killed;
			}
			freeaddrinfo(res[1]);
		} else {
			if (ioctl(dev, DIOCKILLSRCNODES, &psnk))
				err(1, "DIOCKILLSRCNODES");
			killed += psnk.psnk_killed;
		}
	}

	freeaddrinfo(res[0]);

	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "killed %d src nodes from %d sources and %d "
		    "destinations\n", killed, sources, dests);
	return (0);
}

int
pfctl_net_kill_states(int dev, const char *iface, int opts)
{
	struct pfioc_state_kill psk;
	struct addrinfo *res[2], *resp[2];
	struct sockaddr last_src, last_dst;
	int killed, sources, dests;
	int ret_ga;

	killed = sources = dests = 0;

	memset(&psk, 0, sizeof(psk));
	memset(&psk.psk_src.addr.v.a.mask, 0xff,
	    sizeof(psk.psk_src.addr.v.a.mask));
	memset(&last_src, 0xff, sizeof(last_src));
	memset(&last_dst, 0xff, sizeof(last_dst));
	if (iface != NULL && strlcpy(psk.psk_ifname, iface,
	    sizeof(psk.psk_ifname)) >= sizeof(psk.psk_ifname))
		errx(1, "invalid interface: %s", iface);

	pfctl_addrprefix(state_kill[0], &psk.psk_src.addr.v.a.mask);

	if ((ret_ga = getaddrinfo(state_kill[0], NULL, NULL, &res[0]))) {
		errx(1, "getaddrinfo: %s", gai_strerror(ret_ga));
		/* NOTREACHED */
	}
	for (resp[0] = res[0]; resp[0]; resp[0] = resp[0]->ai_next) {
		if (resp[0]->ai_addr == NULL)
			continue;
		/* We get lots of duplicates.  Catch the easy ones */
		if (memcmp(&last_src, resp[0]->ai_addr, sizeof(last_src)) == 0)
			continue;
		last_src = *(struct sockaddr *)resp[0]->ai_addr;

		psk.psk_af = resp[0]->ai_family;
		sources++;

		if (psk.psk_af == AF_INET)
			psk.psk_src.addr.v.a.addr.v4 =
			    ((struct sockaddr_in *)resp[0]->ai_addr)->sin_addr;
		else if (psk.psk_af == AF_INET6)
			psk.psk_src.addr.v.a.addr.v6 =
			    ((struct sockaddr_in6 *)resp[0]->ai_addr)->
			    sin6_addr;
		else
			errx(1, "Unknown address family %d", psk.psk_af);

		if (state_killers > 1) {
			dests = 0;
			memset(&psk.psk_dst.addr.v.a.mask, 0xff,
			    sizeof(psk.psk_dst.addr.v.a.mask));
			memset(&last_dst, 0xff, sizeof(last_dst));
			pfctl_addrprefix(state_kill[1],
			    &psk.psk_dst.addr.v.a.mask);
			if ((ret_ga = getaddrinfo(state_kill[1], NULL, NULL,
			    &res[1]))) {
				errx(1, "getaddrinfo: %s",
				    gai_strerror(ret_ga));
				/* NOTREACHED */
			}
			for (resp[1] = res[1]; resp[1];
			    resp[1] = resp[1]->ai_next) {
				if (resp[1]->ai_addr == NULL)
					continue;
				if (psk.psk_af != resp[1]->ai_family)
					continue;

				if (memcmp(&last_dst, resp[1]->ai_addr,
				    sizeof(last_dst)) == 0)
					continue;
				last_dst = *(struct sockaddr *)resp[1]->ai_addr;

				dests++;

				if (psk.psk_af == AF_INET)
					psk.psk_dst.addr.v.a.addr.v4 =
					    ((struct sockaddr_in *)resp[1]->
					    ai_addr)->sin_addr;
				else if (psk.psk_af == AF_INET6)
					psk.psk_dst.addr.v.a.addr.v6 =
					    ((struct sockaddr_in6 *)resp[1]->
					    ai_addr)->sin6_addr;
				else
					errx(1, "Unknown address family %d",
					    psk.psk_af);

				if (ioctl(dev, DIOCKILLSTATES, &psk))
					err(1, "DIOCKILLSTATES");
				killed += psk.psk_killed;
			}
			freeaddrinfo(res[1]);
		} else {
			if (ioctl(dev, DIOCKILLSTATES, &psk))
				err(1, "DIOCKILLSTATES");
			killed += psk.psk_killed;
		}
	}

	freeaddrinfo(res[0]);

	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "killed %d states from %d sources and %d "
		    "destinations\n", killed, sources, dests);
	return (0);
}

int
pfctl_label_kill_states(int dev, const char *iface, int opts)
{
	struct pfioc_state_kill psk;

	if (state_killers != 2 || (strlen(state_kill[1]) == 0)) {
		warnx("no label specified");
		usage();
	}
	memset(&psk, 0, sizeof(psk));
	if (iface != NULL && strlcpy(psk.psk_ifname, iface,
	    sizeof(psk.psk_ifname)) >= sizeof(psk.psk_ifname))
		errx(1, "invalid interface: %s", iface);

	if (strlcpy(psk.psk_label, state_kill[1], sizeof(psk.psk_label)) >=
	    sizeof(psk.psk_label))
		errx(1, "label too long: %s", state_kill[1]);

	if (ioctl(dev, DIOCKILLSTATES, &psk))
		err(1, "DIOCKILLSTATES");

	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "killed %d states\n", psk.psk_killed);

	return (0);
}

int
pfctl_id_kill_states(int dev, const char *iface, int opts)
{
	struct pfioc_state_kill psk;

	if (state_killers != 2 || (strlen(state_kill[1]) == 0)) {
		warnx("no id specified");
		usage();
	}

	memset(&psk, 0, sizeof(psk));
	if ((sscanf(state_kill[1], "%llx/%x",
	    &psk.psk_pfcmp.id, &psk.psk_pfcmp.creatorid)) == 2)
		HTONL(psk.psk_pfcmp.creatorid);
	else if ((sscanf(state_kill[1], "%llx", &psk.psk_pfcmp.id)) == 1) {
		psk.psk_pfcmp.creatorid = 0;
	} else {
		warnx("wrong id format specified");
		usage();
	}
	if (psk.psk_pfcmp.id == 0) {
		warnx("cannot kill id 0");
		usage();
	}

	psk.psk_pfcmp.id = htobe64(psk.psk_pfcmp.id);
	if (ioctl(dev, DIOCKILLSTATES, &psk))
		err(1, "DIOCKILLSTATES");

	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "killed %d states\n", psk.psk_killed);

	return (0);
}

void
pfctl_print_rule_counters(struct pf_rule *rule, int opts)
{
	if (opts & PF_OPT_DEBUG) {
		const char *t[PF_SKIP_COUNT] = { "i", "d", "r", "f",
		    "p", "sa", "da", "sp", "dp" };
		int i;

		printf("  [ Skip steps: ");
		for (i = 0; i < PF_SKIP_COUNT; ++i) {
			if (rule->skip[i].nr == rule->nr + 1)
				continue;
			printf("%s=", t[i]);
			if (rule->skip[i].nr == -1)
				printf("end ");
			else
				printf("%u ", rule->skip[i].nr);
		}
		printf("]\n");

		printf("  [ queue: qname=%s qid=%u pqname=%s pqid=%u ]\n",
		    rule->qname, rule->qid, rule->pqname, rule->pqid);
	}
	if (opts & PF_OPT_VERBOSE) {
		printf("  [ Evaluations: %-8llu  Packets: %-8llu  "
			    "Bytes: %-10llu  States: %-6u]\n",
			    (unsigned long long)rule->evaluations,
			    (unsigned long long)(rule->packets[0] +
			    rule->packets[1]),
			    (unsigned long long)(rule->bytes[0] +
			    rule->bytes[1]), rule->states_cur);
		if (!(opts & PF_OPT_DEBUG))
			printf("  [ Inserted: uid %lu pid %lu "
			    "State Creations: %-6u]\n",
			    (unsigned long)rule->cuid, (unsigned long)rule->cpid,
			    rule->states_tot);
	}
}

void
pfctl_print_title(char *title)
{
	if (!first_title)
		printf("\n");
	first_title = 0;
	printf("%s\n", title);
}

int
pfctl_show_rules(int dev, char *path, int opts, enum pfctl_show format,
    char *anchorname, int depth, int wildcard, long shownr)
{
	struct pfioc_rule pr;
	u_int32_t nr, mnr, header = 0;
	int len = strlen(path), ret = 0;
	char *npath, *p;

	/*
	 * Truncate a trailing / and * on an anchorname before searching for
	 * the ruleset, this is syntactic sugar that doesn't actually make it
	 * to the kernel.
	 */
	if ((p = strrchr(anchorname, '/')) != NULL &&
	    p[1] == '*' && p[2] == '\0') {
		p[0] = '\0';
	}

	memset(&pr, 0, sizeof(pr));
	if (anchorname[0] == '/') {
		if ((npath = calloc(1, MAXPATHLEN)) == NULL)
			errx(1, "pfctl_rules: calloc");
		strlcpy(npath, anchorname, MAXPATHLEN);
	} else {
		if (path[0])
			snprintf(&path[len], MAXPATHLEN - len, "/%s", anchorname);
		else
			snprintf(&path[len], MAXPATHLEN - len, "%s", anchorname);
		npath = path;
	}

	/*
	 * If this anchor was called with a wildcard path, go through
	 * the rulesets in the anchor rather than the rules.
	 */
	if (wildcard && (opts & PF_OPT_RECURSE)) {
		struct pfioc_ruleset	 prs;
		u_int32_t		 mnr, nr;

		memset(&prs, 0, sizeof(prs));
		memcpy(prs.path, npath, sizeof(prs.path));
		if (ioctl(dev, DIOCGETRULESETS, &prs)) {
			if (errno == EINVAL)
				fprintf(stderr, "Anchor '%s' "
				    "not found.\n", anchorname);
			else
				err(1, "DIOCGETRULESETS");
		}
		mnr = prs.nr;

		pfctl_print_rule_counters(&pr.rule, opts);
		for (nr = 0; nr < mnr; ++nr) {
			prs.nr = nr;
			if (ioctl(dev, DIOCGETRULESET, &prs))
				err(1, "DIOCGETRULESET");
			INDENT(depth, !(opts & PF_OPT_VERBOSE));
			printf("anchor \"%s\" all {\n", prs.name);
			pfctl_show_rules(dev, npath, opts,
			    format, prs.name, depth + 1, 0, shownr);
			INDENT(depth, !(opts & PF_OPT_VERBOSE));
			printf("}\n");
		}
		path[len] = '\0';
		return (0);
	}

	memcpy(pr.anchor, npath, sizeof(pr.anchor));
	if (opts & PF_OPT_SHOWALL) {
		pr.rule.action = PF_PASS;
		if (ioctl(dev, DIOCGETRULES, &pr)) {
			warn("DIOCGETRULES");
			ret = -1;
			goto error;
		}
		header++;
		if (format == PFCTL_SHOW_RULES && (pr.nr > 0 || header))
			pfctl_print_title("FILTER RULES:");
		else if (format == PFCTL_SHOW_LABELS && labels)
			pfctl_print_title("LABEL COUNTERS:");
	}
	if (opts & PF_OPT_CLRRULECTRS)
		pr.action = PF_GET_CLR_CNTR;

	pr.rule.action = PF_PASS;
	if (ioctl(dev, DIOCGETRULES, &pr)) {
		warn("DIOCGETRULES");
		ret = -1;
		goto error;
	}

	if (shownr < 0) {
		mnr = pr.nr;
		nr = 0;
	} else if (shownr < pr.nr) {
		nr = shownr;
		mnr = shownr + 1;
	} else {
		warnx("rule %ld not found", shownr);
		ret = -1;
		goto error;
	}
	for (; nr < mnr; ++nr) {
		pr.nr = nr;
		if (ioctl(dev, DIOCGETRULE, &pr)) {
			warn("DIOCGETRULE");
			ret = -1;
			goto error;
		}

		switch (format) {
		case PFCTL_SHOW_LABELS:
			if (pr.rule.label[0]) {
				INDENT(depth, !(opts & PF_OPT_VERBOSE));
				printf("%s %llu %llu %llu %llu"
				    " %llu %llu %llu %llu\n",
				    pr.rule.label,
				    (unsigned long long)pr.rule.evaluations,
				    (unsigned long long)(pr.rule.packets[0] +
				    pr.rule.packets[1]),
				    (unsigned long long)(pr.rule.bytes[0] +
				    pr.rule.bytes[1]),
				    (unsigned long long)pr.rule.packets[0],
				    (unsigned long long)pr.rule.bytes[0],
				    (unsigned long long)pr.rule.packets[1],
				    (unsigned long long)pr.rule.bytes[1],
				    (unsigned long long)pr.rule.states_tot);
			}
			break;
		case PFCTL_SHOW_RULES:
			if (pr.rule.label[0] && (opts & PF_OPT_SHOWALL))
				labels = 1;
			INDENT(depth, !(opts & PF_OPT_VERBOSE));
			print_rule(&pr.rule, pr.anchor_call, opts);

			/*
			 * If this is a 'unnamed' brace notation
			 * anchor, OR the user has explicitly requested
			 * recursion, print it recursively.
			 */
       		        if (pr.anchor_call[0] &&
			    (((p = strrchr(pr.anchor_call, '/')) ?
			    p[1] == '_' : pr.anchor_call[0] == '_') ||
			    opts & PF_OPT_RECURSE)) {
				printf(" {\n");
				pfctl_print_rule_counters(&pr.rule, opts);
				pfctl_show_rules(dev, npath, opts, format,
				    pr.anchor_call, depth + 1,
				    pr.rule.anchor_wildcard, -1);
				INDENT(depth, !(opts & PF_OPT_VERBOSE));
				printf("}\n");
			} else {
				printf("\n");
				pfctl_print_rule_counters(&pr.rule, opts);
			}
			break;
		case PFCTL_SHOW_NOTHING:
			break;
		}
	}

 error:
	if (path != npath)
		free(npath);
	path[len] = '\0';
	return (ret);
}

int
pfctl_show_src_nodes(int dev, int opts)
{
	struct pfioc_src_nodes psn;
	struct pf_src_node *p;
	char *inbuf = NULL, *newinbuf = NULL;
	unsigned int len = 0;
	int i;

	memset(&psn, 0, sizeof(psn));
	for (;;) {
		psn.psn_len = len;
		if (len) {
			newinbuf = realloc(inbuf, len);
			if (newinbuf == NULL)
				err(1, "realloc");
			psn.psn_buf = inbuf = newinbuf;
		}
		if (ioctl(dev, DIOCGETSRCNODES, &psn) < 0) {
			warn("DIOCGETSRCNODES");
			free(inbuf);
			return (-1);
		}
		if (psn.psn_len + sizeof(struct pfioc_src_nodes) < len)
			break;
		if (len == 0 && psn.psn_len == 0)
			goto done;
		if (len == 0 && psn.psn_len != 0)
			len = psn.psn_len;
		if (psn.psn_len == 0)
			goto done;	/* no src_nodes */
		len *= 2;
	}
	p = psn.psn_src_nodes;
	if (psn.psn_len > 0 && (opts & PF_OPT_SHOWALL))
		pfctl_print_title("SOURCE TRACKING NODES:");
	for (i = 0; i < psn.psn_len; i += sizeof(*p)) {
		print_src_node(p, opts);
		p++;
	}
done:
	free(inbuf);
	return (0);
}

int
pfctl_show_states(int dev, const char *iface, int opts)
{
	struct pfioc_states ps;
	struct pfsync_state *p;
	char *inbuf = NULL, *newinbuf = NULL;
	unsigned int len = 0;
	int i, dotitle = (opts & PF_OPT_SHOWALL);

	memset(&ps, 0, sizeof(ps));
	for (;;) {
		ps.ps_len = len;
		if (len) {
			newinbuf = realloc(inbuf, len);
			if (newinbuf == NULL)
				err(1, "realloc");
			ps.ps_buf = inbuf = newinbuf;
		}
		if (ioctl(dev, DIOCGETSTATES, &ps) < 0) {
			warn("DIOCGETSTATES");
			free(inbuf);
			return (-1);
		}
		if (ps.ps_len + sizeof(struct pfioc_states) < len)
			break;
		if (len == 0 && ps.ps_len == 0)
			goto done;
		if (len == 0 && ps.ps_len != 0)
			len = ps.ps_len;
		if (ps.ps_len == 0)
			goto done;	/* no states */
		len *= 2;
	}
	p = ps.ps_states;
	for (i = 0; i < ps.ps_len; i += sizeof(*p), p++) {
		if (iface != NULL && strcmp(p->ifname, iface))
			continue;
		if (dotitle) {
			pfctl_print_title("STATES:");
			dotitle = 0;
		}
		print_state(p, opts);
	}
done:
	free(inbuf);
	return (0);
}

int
pfctl_show_status(int dev, int opts)
{
	struct pf_status status;

	if (ioctl(dev, DIOCGETSTATUS, &status)) {
		warn("DIOCGETSTATUS");
		return (-1);
	}
	if (opts & PF_OPT_SHOWALL)
		pfctl_print_title("INFO:");
	print_status(&status, opts);
	return (0);
}

int
pfctl_show_timeouts(int dev, int opts)
{
	struct pfioc_tm pt;
	int i;

	if (opts & PF_OPT_SHOWALL)
		pfctl_print_title("TIMEOUTS:");
	memset(&pt, 0, sizeof(pt));
	for (i = 0; pf_timeouts[i].name; i++) {
		pt.timeout = pf_timeouts[i].timeout;
		if (ioctl(dev, DIOCGETTIMEOUT, &pt))
			err(1, "DIOCGETTIMEOUT");
		printf("%-20s %10d", pf_timeouts[i].name, pt.seconds);
		if (pf_timeouts[i].timeout >= PFTM_ADAPTIVE_START &&
		    pf_timeouts[i].timeout <= PFTM_ADAPTIVE_END)
			printf(" states");
		else
			printf("s");
		printf("\n");
	}
	return (0);

}

int
pfctl_show_limits(int dev, int opts)
{
	struct pfioc_limit pl;
	int i;

	if (opts & PF_OPT_SHOWALL)
		pfctl_print_title("LIMITS:");
	memset(&pl, 0, sizeof(pl));
	for (i = 0; pf_limits[i].name; i++) {
		pl.index = pf_limits[i].index;
		if (ioctl(dev, DIOCGETLIMIT, &pl))
			err(1, "DIOCGETLIMIT");
		printf("%-13s ", pf_limits[i].name);
		if (pl.limit == UINT_MAX)
			printf("unlimited\n");
		else
			printf("hard limit %8u\n", pl.limit);
	}
	return (0);
}

/* callbacks for rule/nat/rdr/addr */
int
pfctl_add_rule(struct pfctl *pf, struct pf_rule *r, const char *anchor_call)
{
	struct pf_rule		*rule;
	struct pf_ruleset	*rs;
	char 			*p;

	rs = &pf->anchor->ruleset;
	if (anchor_call[0] && r->anchor == NULL) {
		/*
		 * Don't make non-brace anchors part of the main anchor pool.
		 */
		if ((r->anchor = calloc(1, sizeof(*r->anchor))) == NULL)
			err(1, "pfctl_add_rule: calloc");

		pf_init_ruleset(&r->anchor->ruleset);
		r->anchor->ruleset.anchor = r->anchor;
		if (strlcpy(r->anchor->path, anchor_call,
		    sizeof(rule->anchor->path)) >= sizeof(rule->anchor->path))
                        errx(1, "pfctl_add_rule: strlcpy");
		if ((p = strrchr(anchor_call, '/')) != NULL) {
			if (!strlen(p))
				err(1, "pfctl_add_rule: bad anchor name %s",
				    anchor_call);
		} else
			p = (char *)anchor_call;
		if (strlcpy(r->anchor->name, p,
		    sizeof(rule->anchor->name)) >= sizeof(rule->anchor->name))
                        errx(1, "pfctl_add_rule: strlcpy");
	}

	if ((rule = calloc(1, sizeof(*rule))) == NULL)
		err(1, "calloc");
	bcopy(r, rule, sizeof(*rule));

	TAILQ_INSERT_TAIL(rs->rules.active.ptr, rule, entries);
	return (0);
}

int
pfctl_ruleset_trans(struct pfctl *pf, char *path, struct pf_anchor *a)
{
	int osize = pf->trans->pfrb_size;

	if (a == pf->astack[0] && (altqsupport && loadaltq)) {
		if (pfctl_add_trans(pf->trans, PF_TRANS_ALTQ, path))
			return (2);
	}
	if (pfctl_add_trans(pf->trans, PF_TRANS_RULESET, path))
		return (3);
	if (pfctl_add_trans(pf->trans, PF_TRANS_TABLE, path))
		return (4);
	if (pfctl_trans(pf->dev, pf->trans, DIOCXBEGIN, osize))
		return (5);

	return (0);
}

int
pfctl_load_ruleset(struct pfctl *pf, char *path, struct pf_ruleset *rs,
    int depth)
{
	struct pf_rule *r;
	int		error, len = strlen(path);
	int		brace = 0;

	pf->anchor = rs->anchor;

	if (path[0])
		snprintf(&path[len], MAXPATHLEN - len, "/%s", pf->anchor->name);
	else
		snprintf(&path[len], MAXPATHLEN - len, "%s", pf->anchor->name);

	if (depth) {
		if (TAILQ_FIRST(rs->rules.active.ptr) != NULL) {
			brace++;
			if (pf->opts & PF_OPT_VERBOSE)
				printf(" {\n");
			if ((pf->opts & PF_OPT_NOACTION) == 0 &&
			    (error = pfctl_ruleset_trans(pf,
			    path, rs->anchor))) {
				printf("pfctl_load_rulesets: "
				    "pfctl_ruleset_trans %d\n", error);
				goto error;
			}
		} else if (pf->opts & PF_OPT_VERBOSE)
			printf("\n");

	}

	if (pf->optimize)
		pfctl_optimize_ruleset(pf, rs);

	while ((r = TAILQ_FIRST(rs->rules.active.ptr)) != NULL) {
		TAILQ_REMOVE(rs->rules.active.ptr, r, entries);
		if ((error = pfctl_load_rule(pf, path, r, depth)))
			goto error;
		if (r->anchor) {
			if ((error = pfctl_load_ruleset(pf, path,
			    &r->anchor->ruleset, depth + 1)))
				goto error;
		} else if (pf->opts & PF_OPT_VERBOSE)
			printf("\n");
		free(r);
	}
	if (brace && pf->opts & PF_OPT_VERBOSE) {
		INDENT(depth - 1, (pf->opts & PF_OPT_VERBOSE));
		printf("}\n");
	}
	path[len] = '\0';
	return (0);

 error:
	path[len] = '\0';
	return (error);

}

int
pfctl_load_rule(struct pfctl *pf, char *path, struct pf_rule *r, int depth)
{
	char			*name;
	struct pfioc_rule	pr;
	int			len = strlen(path);

	bzero(&pr, sizeof(pr));
	/* set up anchor before adding to path for anchor_call */
	if ((pf->opts & PF_OPT_NOACTION) == 0)
		pr.ticket = pfctl_get_ticket(pf->trans, PF_TRANS_RULESET, path);
	if (strlcpy(pr.anchor, path, sizeof(pr.anchor)) >= sizeof(pr.anchor))
		errx(1, "pfctl_load_rule: strlcpy");

	if (r->anchor) {
		if (r->anchor->match) {
			if (path[0])
				snprintf(&path[len], MAXPATHLEN - len,
				    "/%s", r->anchor->name);
			else
				snprintf(&path[len], MAXPATHLEN - len,
				    "%s", r->anchor->name);
			name = r->anchor->name;
		} else
			name = r->anchor->path;
	} else
		name = "";

	if ((pf->opts & PF_OPT_NOACTION) == 0) {
		memcpy(&pr.rule, r, sizeof(pr.rule));
		if (r->anchor && strlcpy(pr.anchor_call, name,
		    sizeof(pr.anchor_call)) >= sizeof(pr.anchor_call))
			errx(1, "pfctl_load_rule: strlcpy");
		if (ioctl(pf->dev, DIOCADDRULE, &pr))
			err(1, "DIOCADDRULE");
	}

	if (pf->opts & PF_OPT_VERBOSE) {
		INDENT(depth, !(pf->opts & PF_OPT_VERBOSE2));
		print_rule(r, name, pf->opts);
	}
	path[len] = '\0';
	return (0);
}

int
pfctl_add_altq(struct pfctl *pf, struct pf_altq *a)
{
	if (altqsupport) {
		memcpy(&pf->paltq->altq, a, sizeof(struct pf_altq));
		if ((pf->opts & PF_OPT_NOACTION) == 0) {
			if (ioctl(pf->dev, DIOCADDALTQ, pf->paltq)) {
				if (errno == ENXIO)
					errx(1, "qtype not configured");
				else if (errno == ENODEV)
					errx(1, "%s: driver does not support "
					    "altq", a->ifname);
				else
					err(1, "DIOCADDALTQ");
			}
		}
		pfaltq_store(&pf->paltq->altq);
	}
	return (0);
}

int
pfctl_rules(int dev, char *filename, int opts, int optimize,
    char *anchorname, struct pfr_buffer *trans)
{
#define ERR(x) do { warn(x); goto _error; } while(0)
#define ERRX(x) do { warnx(x); goto _error; } while(0)

	struct pfr_buffer	*t, buf;
	struct pfioc_altq	 pa;
	struct pfctl		 pf;
	struct pf_ruleset	*rs;
	struct pfr_table	 trs;
	char			*path = NULL;
	int			 osize;

	bzero(&pf, sizeof(pf));
	RB_INIT(&pf_anchors);
	memset(&pf_main_anchor, 0, sizeof(pf_main_anchor));
	pf_init_ruleset(&pf_main_anchor.ruleset);
	pf_main_anchor.ruleset.anchor = &pf_main_anchor;
	if (trans == NULL) {
		bzero(&buf, sizeof(buf));
		buf.pfrb_type = PFRB_TRANS;
		t = &buf;
		osize = 0;
	} else {
		t = trans;
		osize = t->pfrb_size;
	}

	memset(&pa, 0, sizeof(pa));
	memset(&pf, 0, sizeof(pf));
	memset(&trs, 0, sizeof(trs));
	if ((path = calloc(1, MAXPATHLEN)) == NULL)
		ERRX("pfctl_rules: calloc");
	if (strlcpy(trs.pfrt_anchor, anchorname,
	    sizeof(trs.pfrt_anchor)) >= sizeof(trs.pfrt_anchor))
		ERRX("pfctl_rules: strlcpy");
	pf.dev = dev;
	pf.opts = opts;
	pf.optimize = optimize;
	if (anchorname[0])
		loadaltq = 0;

	/* non-brace anchor, create without resolving the path */
	if ((pf.anchor = calloc(1, sizeof(*pf.anchor))) == NULL)
		ERRX("pfctl_rules: calloc");
	rs = &pf.anchor->ruleset;
	pf_init_ruleset(rs);
	rs->anchor = pf.anchor;
	if (strlcpy(pf.anchor->path, anchorname,
	    sizeof(pf.anchor->path)) >= sizeof(pf.anchor->path))
		errx(1, "pfctl_add_rule: strlcpy");
	if (strlcpy(pf.anchor->name, anchorname,
	    sizeof(pf.anchor->name)) >= sizeof(pf.anchor->name))
		errx(1, "pfctl_add_rule: strlcpy");


	pf.astack[0] = pf.anchor;
	pf.asd = 0;
	pf.paltq = &pa;
	pf.trans = t;
	pfctl_init_options(&pf);

	if ((opts & PF_OPT_NOACTION) == 0) {
		/*
		 * XXX For the time being we need to open transactions for
		 * the main ruleset before parsing, because tables are still
		 * loaded at parse time.
		 */
		if (pfctl_ruleset_trans(&pf, anchorname, pf.anchor))
			ERRX("pfctl_rules");
		if (altqsupport && loadaltq)
			pa.ticket =
			    pfctl_get_ticket(t, PF_TRANS_ALTQ, anchorname);
		pf.astack[0]->ruleset.tticket =
		    pfctl_get_ticket(t, PF_TRANS_TABLE, anchorname);
	}

	if (parse_config(filename, &pf) < 0) {
		if ((opts & PF_OPT_NOACTION) == 0)
			ERRX("Syntax error in config file: "
			    "pf rules not loaded");
		else
			goto _error;
	}

	if (pfctl_load_ruleset(&pf, path, rs, 0)) {
		if ((opts & PF_OPT_NOACTION) == 0)
			ERRX("Unable to load rules into kernel");
		else
			goto _error;
	}

	free(path);

	if (altqsupport && loadaltq && check_commit_altq(dev, opts) != 0)
		ERRX("errors in altq config");

	/* process "load anchor" directives */
	if (!anchorname[0])
		if (pfctl_load_anchors(dev, &pf, t) == -1)
			ERRX("load anchors");

	if (trans == NULL && (opts & PF_OPT_NOACTION) == 0) {
		if (!anchorname[0])
			if (pfctl_load_options(&pf))
				goto _error;
		if (pfctl_trans(dev, t, DIOCXCOMMIT, osize))
			ERR("DIOCXCOMMIT");
	}
	return (0);

_error:
	if (trans == NULL) {	/* main ruleset */
		if ((opts & PF_OPT_NOACTION) == 0)
			if (pfctl_trans(dev, t, DIOCXROLLBACK, osize))
				err(1, "DIOCXROLLBACK");
		exit(1);
	} else {		/* sub ruleset */
		if (path)
			free(path);
		return (-1);
	}

#undef ERR
#undef ERRX
}

FILE *
pfctl_fopen(const char *name, const char *mode)
{
	struct stat	 st;
	FILE		*fp;

	fp = fopen(name, mode);
	if (fp == NULL)
		return (NULL);
	if (fstat(fileno(fp), &st)) {
		fclose(fp);
		return (NULL);
	}
	if (S_ISDIR(st.st_mode)) {
		fclose(fp);
		errno = EISDIR;
		return (NULL);
	}
	return (fp);
}

void
pfctl_init_options(struct pfctl *pf)
{
	int64_t mem;
	int mib[2], mcl;
	size_t size;

	pf->timeout[PFTM_TCP_FIRST_PACKET] = PFTM_TCP_FIRST_PACKET_VAL;
	pf->timeout[PFTM_TCP_OPENING] = PFTM_TCP_OPENING_VAL;
	pf->timeout[PFTM_TCP_ESTABLISHED] = PFTM_TCP_ESTABLISHED_VAL;
	pf->timeout[PFTM_TCP_CLOSING] = PFTM_TCP_CLOSING_VAL;
	pf->timeout[PFTM_TCP_FIN_WAIT] = PFTM_TCP_FIN_WAIT_VAL;
	pf->timeout[PFTM_TCP_CLOSED] = PFTM_TCP_CLOSED_VAL;
	pf->timeout[PFTM_UDP_FIRST_PACKET] = PFTM_UDP_FIRST_PACKET_VAL;
	pf->timeout[PFTM_UDP_SINGLE] = PFTM_UDP_SINGLE_VAL;
	pf->timeout[PFTM_UDP_MULTIPLE] = PFTM_UDP_MULTIPLE_VAL;
	pf->timeout[PFTM_ICMP_FIRST_PACKET] = PFTM_ICMP_FIRST_PACKET_VAL;
	pf->timeout[PFTM_ICMP_ERROR_REPLY] = PFTM_ICMP_ERROR_REPLY_VAL;
	pf->timeout[PFTM_OTHER_FIRST_PACKET] = PFTM_OTHER_FIRST_PACKET_VAL;
	pf->timeout[PFTM_OTHER_SINGLE] = PFTM_OTHER_SINGLE_VAL;
	pf->timeout[PFTM_OTHER_MULTIPLE] = PFTM_OTHER_MULTIPLE_VAL;
	pf->timeout[PFTM_FRAG] = PFTM_FRAG_VAL;
	pf->timeout[PFTM_INTERVAL] = PFTM_INTERVAL_VAL;
	pf->timeout[PFTM_SRC_NODE] = PFTM_SRC_NODE_VAL;
	pf->timeout[PFTM_TS_DIFF] = PFTM_TS_DIFF_VAL;
	pf->timeout[PFTM_ADAPTIVE_START] = PFSTATE_ADAPT_START;
	pf->timeout[PFTM_ADAPTIVE_END] = PFSTATE_ADAPT_END;

	pf->limit[PF_LIMIT_STATES] = PFSTATE_HIWAT;

	mib[0] = CTL_KERN;
	mib[1] = KERN_MAXCLUSTERS;
	size = sizeof(mcl);
	if (sysctl(mib, 2, &mcl, &size, NULL, 0) == -1)
		err(1, "sysctl");
	pf->limit[PF_LIMIT_FRAGS] = mcl / 4;

	pf->limit[PF_LIMIT_SRC_NODES] = PFSNODE_HIWAT;
	pf->limit[PF_LIMIT_TABLES] = PFR_KTABLE_HIWAT;
	pf->limit[PF_LIMIT_TABLE_ENTRIES] = PFR_KENTRY_HIWAT;

	mib[0] = CTL_HW;
	mib[1] = HW_PHYSMEM64;
	size = sizeof(mem);
	if (sysctl(mib, 2, &mem, &size, NULL, 0) == -1)
		err(1, "sysctl");
	if (mem <= 100*1024*1024)
		pf->limit[PF_LIMIT_TABLE_ENTRIES] = PFR_KENTRY_HIWAT_SMALL;

	pf->debug = LOG_ERR;
	pf->debug_set = 0;
	pf->reassemble = PF_REASS_ENABLED;
}

int
pfctl_load_options(struct pfctl *pf)
{
	int i, error = 0;

	/* load limits */
	for (i = 0; i < PF_LIMIT_MAX; i++)
		if (pfctl_load_limit(pf, i, pf->limit[i]))
			error = 1;

	/*
	 * If we've set the limit, but haven't explicitly set adaptive
	 * timeouts, do it now with a start of 60% and end of 120%.
	 */
	if (pf->limit_set[PF_LIMIT_STATES] &&
	    !pf->timeout_set[PFTM_ADAPTIVE_START] &&
	    !pf->timeout_set[PFTM_ADAPTIVE_END]) {
		pf->timeout[PFTM_ADAPTIVE_START] =
			(pf->limit[PF_LIMIT_STATES] / 10) * 6;
		pf->timeout_set[PFTM_ADAPTIVE_START] = 1;
		pf->timeout[PFTM_ADAPTIVE_END] =
			(pf->limit[PF_LIMIT_STATES] / 10) * 12;
		pf->timeout_set[PFTM_ADAPTIVE_END] = 1;
	}

	/* load timeouts */
	for (i = 0; i < PFTM_MAX; i++)
		if (pfctl_load_timeout(pf, i, pf->timeout[i]))
			error = 1;

	/* load debug */
	if (pf->debug_set && pfctl_load_debug(pf, pf->debug))
		error = 1;

	/* load logif */
	if (pf->ifname_set && pfctl_load_logif(pf, pf->ifname))
		error = 1;

	/* load hostid */
	if (pf->hostid_set && pfctl_load_hostid(pf, pf->hostid))
		error = 1;

	/* load reassembly settings */
	if (pf->reass_set && pfctl_load_reassembly(pf, pf->reassemble))
		error = 1;

	return (error);
}

int
pfctl_set_limit(struct pfctl *pf, const char *opt, unsigned int limit)
{
	int i;


	for (i = 0; pf_limits[i].name; i++) {
		if (strcasecmp(opt, pf_limits[i].name) == 0) {
			pf->limit[pf_limits[i].index] = limit;
			pf->limit_set[pf_limits[i].index] = 1;
			break;
		}
	}
	if (pf_limits[i].name == NULL) {
		warnx("Bad pool name.");
		return (1);
	}

	if (pf->opts & PF_OPT_VERBOSE)
		printf("set limit %s %d\n", opt, limit);

	return (0);
}

int
pfctl_load_limit(struct pfctl *pf, unsigned int index, unsigned int limit)
{
	struct pfioc_limit pl;

	memset(&pl, 0, sizeof(pl));
	pl.index = index;
	pl.limit = limit;
	if (ioctl(pf->dev, DIOCSETLIMIT, &pl)) {
		if (errno == EBUSY)
			warnx("Current pool size exceeds requested hard limit");
		else
			warnx("cannot set '%s' limit", pf_limits[index].name);
		return (1);
	}
	return (0);
}

int
pfctl_set_timeout(struct pfctl *pf, const char *opt, int seconds, int quiet)
{
	int i;

	for (i = 0; pf_timeouts[i].name; i++) {
		if (strcasecmp(opt, pf_timeouts[i].name) == 0) {
			pf->timeout[pf_timeouts[i].timeout] = seconds;
			pf->timeout_set[pf_timeouts[i].timeout] = 1;
			break;
		}
	}

	if (pf_timeouts[i].name == NULL) {
		warnx("Bad timeout name.");
		return (1);
	}


	if (pf->opts & PF_OPT_VERBOSE && ! quiet)
		printf("set timeout %s %d\n", opt, seconds);

	return (0);
}

int
pfctl_load_timeout(struct pfctl *pf, unsigned int timeout, unsigned int seconds)
{
	struct pfioc_tm pt;

	memset(&pt, 0, sizeof(pt));
	pt.timeout = timeout;
	pt.seconds = seconds;
	if (ioctl(pf->dev, DIOCSETTIMEOUT, &pt)) {
		warnx("DIOCSETTIMEOUT");
		return (1);
	}
	return (0);
}

int
pfctl_set_reassembly(struct pfctl *pf, int on, int nodf)
{
	pf->reass_set = 1;
	if (on) {
		pf->reassemble = PF_REASS_ENABLED;
		if (nodf)
			pf->reassemble |= PF_REASS_NODF;
	} else {
		pf->reassemble = 0;
	}

	if (pf->opts & PF_OPT_VERBOSE)
		printf("set reassemble %s %s\n", on ? "yes" : "no",
		    nodf ? "no-df" : "");

	return (0);
}

int
pfctl_set_optimization(struct pfctl *pf, const char *opt)
{
	const struct pf_hint *hint;
	int i, r;

	for (i = 0; pf_hints[i].name; i++)
		if (strcasecmp(opt, pf_hints[i].name) == 0)
			break;

	hint = pf_hints[i].hint;
	if (hint == NULL) {
		warnx("invalid state timeouts optimization");
		return (1);
	}

	for (i = 0; hint[i].name; i++)
		if ((r = pfctl_set_timeout(pf, hint[i].name,
		    hint[i].timeout, 1)))
			return (r);

	if (pf->opts & PF_OPT_VERBOSE)
		printf("set optimization %s\n", opt);

	return (0);
}

int
pfctl_set_logif(struct pfctl *pf, char *ifname)
{
	if (!strcmp(ifname, "none")) {
		free(pf->ifname);
		pf->ifname = NULL;
	} else {
		pf->ifname = strdup(ifname);
		if (!pf->ifname)
			errx(1, "pfctl_set_logif: strdup");
	}
	pf->ifname_set = 1;

	if (pf->opts & PF_OPT_VERBOSE)
		printf("set loginterface %s\n", ifname);

	return (0);
}

int
pfctl_load_logif(struct pfctl *pf, char *ifname)
{
	struct pfioc_iface	pi;

	memset(&pi, 0, sizeof(pi));
	if (ifname && strlcpy(pi.pfiio_name, ifname,
	    sizeof(pi.pfiio_name)) >= sizeof(pi.pfiio_name)) {
		warnx("pfctl_load_logif: strlcpy");
		return (1);
	}
	if (ioctl(pf->dev, DIOCSETSTATUSIF, &pi)) {
		warnx("DIOCSETSTATUSIF");
		return (1);
	}
	return (0);
}

void
pfctl_set_hostid(struct pfctl *pf, u_int32_t hostid)
{
	HTONL(hostid);

	pf->hostid = hostid;
	pf->hostid_set = 1;

	if (pf->opts & PF_OPT_VERBOSE)
		printf("set hostid 0x%08x\n", ntohl(hostid));
}

int
pfctl_load_hostid(struct pfctl *pf, u_int32_t hostid)
{
	if (ioctl(dev, DIOCSETHOSTID, &hostid)) {
		warnx("DIOCSETHOSTID");
		return (1);
	}
	return (0);
}

int
pfctl_load_reassembly(struct pfctl *pf, u_int32_t reassembly)
{
	if (ioctl(dev, DIOCSETREASS, &reassembly)) {
		warnx("DIOCSETREASS");
		return (1);
	}
	return (0);
}

int
pfctl_set_debug(struct pfctl *pf, char *d)
{
	u_int32_t	level;
	int		loglevel;

	if (!strcmp(d, "none"))
		level = LOG_CRIT;
	else if (!strcmp(d, "urgent"))
		level = LOG_ERR;
	else if (!strcmp(d, "misc"))
		level = LOG_NOTICE;
	else if (!strcmp(d, "loud"))
		level = LOG_DEBUG;
	else if ((loglevel = string_to_loglevel(d)) >= 0)
		level = loglevel;
	else {
		warnx("unknown debug level \"%s\"", d);
		return (-1);
	}
	pf->debug = level;
	pf->debug_set = 1;

	if ((pf->opts & PF_OPT_NOACTION) == 0)
		if (ioctl(dev, DIOCSETDEBUG, &level))
			err(1, "DIOCSETDEBUG");

	if (pf->opts & PF_OPT_VERBOSE)
		printf("set debug %s\n", d);

	return (0);
}

int
pfctl_load_debug(struct pfctl *pf, unsigned int level)
{
	if (ioctl(pf->dev, DIOCSETDEBUG, &level)) {
		warnx("DIOCSETDEBUG");
		return (1);
	}
	return (0);
}

int
pfctl_set_interface_flags(struct pfctl *pf, char *ifname, int flags, int how)
{
	struct pfioc_iface	pi;

	bzero(&pi, sizeof(pi));

	pi.pfiio_flags = flags;

	if (strlcpy(pi.pfiio_name, ifname, sizeof(pi.pfiio_name)) >=
	    sizeof(pi.pfiio_name))
		errx(1, "pfctl_set_interface_flags: strlcpy");

	if ((pf->opts & PF_OPT_NOACTION) == 0) {
		if (how == 0) {
			if (ioctl(pf->dev, DIOCCLRIFFLAG, &pi))
				err(1, "DIOCCLRIFFLAG");
		} else {
			if (ioctl(pf->dev, DIOCSETIFFLAG, &pi))
				err(1, "DIOCSETIFFLAG");
		}
	}
	return (0);
}

void
pfctl_debug(int dev, u_int32_t level, int opts)
{
	struct pfr_buffer t;

	memset(&t, 0, sizeof(t));
	t.pfrb_type = PFRB_TRANS;
	if (pfctl_trans(dev, &t, DIOCXBEGIN, 0) ||
	    ioctl(dev, DIOCSETDEBUG, &level) ||
	    pfctl_trans(dev, &t, DIOCXCOMMIT, 0))
		err(1, "pfctl_debug ioctl");

	if ((opts & PF_OPT_QUIET) == 0)
		fprintf(stderr, "debug level set to '%s'\n",
		    loglevel_to_string(level));
}

int
pfctl_test_altqsupport(int dev, int opts)
{
	struct pfioc_altq pa;

	if (ioctl(dev, DIOCGETALTQS, &pa)) {
		if (errno == ENODEV) {
			if (!(opts & PF_OPT_QUIET))
				fprintf(stderr, "No ALTQ support in kernel\n"
				    "ALTQ related functions disabled\n");
			return (0);
		} else
			err(1, "DIOCGETALTQS");
	}
	return (1);
}

int
pfctl_show_anchors(int dev, int opts, char *anchorname)
{
	struct pfioc_ruleset	 pr;
	u_int32_t		 mnr, nr;

	memset(&pr, 0, sizeof(pr));
	memcpy(pr.path, anchorname, sizeof(pr.path));
	if (ioctl(dev, DIOCGETRULESETS, &pr)) {
		if (errno == EINVAL)
			fprintf(stderr, "Anchor '%s' not found.\n",
			    anchorname);
		else
			err(1, "DIOCGETRULESETS");
		return (-1);
	}
	mnr = pr.nr;
	for (nr = 0; nr < mnr; ++nr) {
		char sub[MAXPATHLEN];

		pr.nr = nr;
		if (ioctl(dev, DIOCGETRULESET, &pr))
			err(1, "DIOCGETRULESET");
		if (!strcmp(pr.name, PF_RESERVED_ANCHOR))
			continue;
		sub[0] = 0;
		if (pr.path[0]) {
			strlcat(sub, pr.path, sizeof(sub));
			strlcat(sub, "/", sizeof(sub));
		}
		strlcat(sub, pr.name, sizeof(sub));
		if (sub[0] != '_' || (opts & PF_OPT_VERBOSE))
			printf("  %s\n", sub);
		if ((opts & PF_OPT_VERBOSE) && pfctl_show_anchors(dev, opts, sub))
			return (-1);
	}
	return (0);
}

const char *
pfctl_lookup_option(char *cmd, const char **list)
{
	const char *item = NULL;
	if (cmd != NULL && *cmd)
		for (; *list; list++)
			if (!strncmp(cmd, *list, strlen(cmd))) {
				if (item == NULL)
					item = *list;
				else
					errx(1, "%s is ambigious", cmd);
			}

	return (item);
}


void
pfctl_state_store(int dev, const char *file)
{
	FILE *f;
	struct pfioc_states ps;
	char *inbuf = NULL, *newinbuf = NULL;
	unsigned int len = 0;
	size_t n;

	f = fopen(file, "w");
	if (f == NULL)
		err(1, "open: %s", file);

	memset(&ps, 0, sizeof(ps));
	for (;;) {
		ps.ps_len = len;
		if (len) {
			newinbuf = realloc(inbuf, len);
			if (newinbuf == NULL)
				err(1, "realloc");
			ps.ps_buf = inbuf = newinbuf;
		}
		if (ioctl(dev, DIOCGETSTATES, &ps) < 0)
			err(1, "DIOCGETSTATES");

		if (ps.ps_len + sizeof(struct pfioc_states) < len)
			break;
		if (len == 0 && ps.ps_len == 0)
			return;
		if (len == 0 && ps.ps_len != 0)
			len = ps.ps_len;
		if (ps.ps_len == 0)
			return; /* no states */
		len *= 2;
	}

	n = ps.ps_len / sizeof(struct pfsync_state);
	if (fwrite(inbuf, sizeof(struct pfsync_state), n, f) < n)
		err(1, "fwrite");

	fclose(f);
}

void
pfctl_state_load(int dev, const char *file)
{
	FILE *f;
	struct pfioc_state ps;

	f = fopen(file, "r");
	if (f == NULL)
		err(1, "open: %s", file);

	while (fread(&ps.state, sizeof(ps.state), 1, f) == 1) {
		if (ioctl(dev, DIOCADDSTATE, &ps) < 0) {
			switch (errno) {
			case EEXIST:
			case EINVAL:
				break;
			default:
				err(1, "DIOCADDSTATE");
			}
		}
	}

	fclose(f);
}

int
main(int argc, char *argv[])
{
	int	 error = 0;
	int	 ch;
	int	 mode = O_RDONLY;
	int	 opts = 0;
	int	 optimize = PF_OPTIMIZE_BASIC;
	int	 level;
	char	 anchorname[MAXPATHLEN];
	int	 anchor_wildcard = 0;
	char	*path;
	char	*lfile = NULL, *sfile = NULL;
	const char *errstr;
	long	 shownr = -1;

	if (argc < 2)
		usage();

	while ((ch = getopt(argc, argv,
	    "a:dD:eqf:F:ghi:k:K:L:no:Pp:R:rS:s:t:T:vx:z")) != -1) {
		switch (ch) {
		case 'a':
			anchoropt = optarg;
			break;
		case 'd':
			opts |= PF_OPT_DISABLE;
			mode = O_RDWR;
			break;
		case 'D':
			if (pfctl_cmdline_symset(optarg) < 0)
				warnx("could not parse macro definition %s",
				    optarg);
			break;
		case 'e':
			opts |= PF_OPT_ENABLE;
			mode = O_RDWR;
			break;
		case 'q':
			opts |= PF_OPT_QUIET;
			break;
		case 'F':
			clearopt = pfctl_lookup_option(optarg, clearopt_list);
			if (clearopt == NULL) {
				warnx("Unknown flush modifier '%s'", optarg);
				usage();
			}
			mode = O_RDWR;
			break;
		case 'i':
			ifaceopt = optarg;
			break;
		case 'k':
			if (state_killers >= 2) {
				warnx("can only specify -k twice");
				usage();
				/* NOTREACHED */
			}
			state_kill[state_killers++] = optarg;
			mode = O_RDWR;
			break;
		case 'K':
			if (src_node_killers >= 2) {
				warnx("can only specify -K twice");
				usage();
				/* NOTREACHED */
			}
			src_node_kill[src_node_killers++] = optarg;
			mode = O_RDWR;
			break;
		case 'n':
			opts |= PF_OPT_NOACTION;
			break;
		case 'r':
			opts |= PF_OPT_USEDNS;
			break;
		case 'R':
			shownr = strtonum(optarg, -1, LONG_MAX, &errstr);
			if (errstr) {
				warnx("invalid rule id: %s", errstr);
				usage();
			}
			break;
		case 'f':
			rulesopt = optarg;
			mode = O_RDWR;
			break;
		case 'g':
			opts |= PF_OPT_DEBUG;
			break;
		case 'o':
			optiopt = pfctl_lookup_option(optarg, optiopt_list);
			if (optiopt == NULL) {
				warnx("Unknown optimization '%s'", optarg);
				usage();
			}
			opts |= PF_OPT_OPTIMIZE;
			break;
		case 'P':
			opts |= PF_OPT_PORTNAMES;
			break;
		case 'p':
			pf_device = optarg;
			break;
		case 's':
			showopt = pfctl_lookup_option(optarg, showopt_list);
			if (showopt == NULL) {
				warnx("Unknown show modifier '%s'", optarg);
				usage();
			}
			break;
		case 't':
			tableopt = optarg;
			break;
		case 'T':
			tblcmdopt = pfctl_lookup_option(optarg, tblcmdopt_list);
			if (tblcmdopt == NULL) {
				warnx("Unknown table command '%s'", optarg);
				usage();
			}
			break;
		case 'v':
			if (opts & PF_OPT_VERBOSE)
				opts |= PF_OPT_VERBOSE2;
			opts |= PF_OPT_VERBOSE;
			break;
		case 'x':
			debugopt = pfctl_lookup_option(optarg, debugopt_list);
			if (debugopt == NULL) {
				warnx("Unknown debug level '%s'", optarg);
				usage();
			}
			mode = O_RDWR;
			break;
		case 'z':
			opts |= PF_OPT_CLRRULECTRS;
			mode = O_RDWR;
			break;
		case 'S':
			sfile = optarg;
			break;
		case 'L':
			mode = O_RDWR;
			lfile = optarg;
			break;
		case 'h':
			/* FALLTHROUGH */
		default:
			usage();
			/* NOTREACHED */
		}
	}

	if (tblcmdopt != NULL) {
		argc -= optind;
		argv += optind;
		ch = *tblcmdopt;
		mode = strchr("acdefkrz", ch) ? O_RDWR : O_RDONLY;
	} else if (argc != optind) {
		warnx("unknown command line argument: %s ...", argv[optind]);
		usage();
		/* NOTREACHED */
	}

	if ((path = calloc(1, MAXPATHLEN)) == NULL)
		errx(1, "pfctl: calloc");
	memset(anchorname, 0, sizeof(anchorname));
	if (anchoropt != NULL) {
		int len = strlen(anchoropt);

		if (anchoropt[len - 1] == '*') {
			if (len >= 2 && anchoropt[len - 2] == '/') {
				anchoropt[len - 2] = '\0';
				anchor_wildcard = 1;
			} else
				anchoropt[len - 1] = '\0';
			opts |= PF_OPT_RECURSE;
		}
		if (strlcpy(anchorname, anchoropt,
		    sizeof(anchorname)) >= sizeof(anchorname))
			errx(1, "anchor name '%s' too long",
			    anchoropt);
	}

	if ((opts & PF_OPT_NOACTION) == 0) {
		dev = open(pf_device, mode);
		if (dev == -1)
			err(1, "%s", pf_device);
		altqsupport = pfctl_test_altqsupport(dev, opts);
	} else {
		dev = open(pf_device, O_RDONLY);
		if (dev >= 0)
			opts |= PF_OPT_DUMMYACTION;
		/* turn off options */
		opts &= ~ (PF_OPT_DISABLE | PF_OPT_ENABLE);
		clearopt = showopt = debugopt = NULL;
		altqsupport = 1;
	}

	if (opts & PF_OPT_DISABLE)
		if (pfctl_disable(dev, opts))
			error = 1;

	if (showopt != NULL) {
		switch (*showopt) {
		case 'A':
			pfctl_show_anchors(dev, opts, anchorname);
			break;
		case 'r':
			pfctl_load_fingerprints(dev, opts);
			pfctl_show_rules(dev, path, opts, PFCTL_SHOW_RULES,
			    anchorname, 0, 0, shownr);
			if (anchor_wildcard)
				pfctl_show_rules(dev, path, opts,
				    PFCTL_SHOW_RULES, anchorname, 0,
				    anchor_wildcard, shownr);
			break;
		case 'l':
			pfctl_load_fingerprints(dev, opts);
			pfctl_show_rules(dev, path, opts, PFCTL_SHOW_LABELS,
			    anchorname, 0, 0, shownr);
			if (anchor_wildcard)
				pfctl_show_rules(dev, path, opts,
				    PFCTL_SHOW_LABELS, anchorname, 0,
				    anchor_wildcard, shownr);
			break;
		case 'q':
			pfctl_show_altq(dev, ifaceopt, opts,
			    opts & PF_OPT_VERBOSE2);
			break;
		case 's':
			pfctl_show_states(dev, ifaceopt, opts);
			break;
		case 'S':
			pfctl_show_src_nodes(dev, opts);
			break;
		case 'i':
			pfctl_show_status(dev, opts);
			break;
		case 't':
			pfctl_show_timeouts(dev, opts);
			break;
		case 'm':
			pfctl_show_limits(dev, opts);
			break;
		case 'a':
			opts |= PF_OPT_SHOWALL;
			pfctl_load_fingerprints(dev, opts);

			pfctl_show_rules(dev, path, opts, 0, anchorname,
			    0, 0, -1);
			pfctl_show_altq(dev, ifaceopt, opts, 0);
			pfctl_show_states(dev, ifaceopt, opts);
			pfctl_show_src_nodes(dev, opts);
			pfctl_show_status(dev, opts);
			pfctl_show_rules(dev, path, opts, 1, anchorname,
			    0, 0, -1);
			pfctl_show_timeouts(dev, opts);
			pfctl_show_limits(dev, opts);
			pfctl_show_tables(anchorname, opts);
			pfctl_show_fingerprints(opts);
			break;
		case 'T':
			pfctl_show_tables(anchorname, opts);
			break;
		case 'o':
			pfctl_load_fingerprints(dev, opts);
			pfctl_show_fingerprints(opts);
			break;
		case 'I':
			pfctl_show_ifaces(ifaceopt, opts);
			break;
		}
	}

	if ((opts & PF_OPT_CLRRULECTRS) && showopt == NULL)
		pfctl_show_rules(dev, path, opts, PFCTL_SHOW_NOTHING,
		    anchorname, 0, 0, -1);

	if (clearopt != NULL) {
		if (anchorname[0] == '_' || strstr(anchorname, "/_") != NULL)
			errx(1, "anchor names beginning with '_' cannot "
			    "be modified from the command line");

		switch (*clearopt) {
		case 'r':
			pfctl_clear_rules(dev, opts, anchorname);
			break;
		case 'q':
			pfctl_clear_altq(dev, opts);
			break;
		case 's':
			pfctl_clear_states(dev, ifaceopt, opts);
			break;
		case 'S':
			pfctl_clear_src_nodes(dev, opts);
			break;
		case 'i':
			pfctl_clear_stats(dev, ifaceopt, opts);
			break;
		case 'a':
			pfctl_clear_rules(dev, opts, anchorname);
			pfctl_clear_tables(anchorname, opts);
			if (ifaceopt && *ifaceopt) {
				warnx("don't specify an interface with -Fall");
				usage();
				/* NOTREACHED */
			}
			if (!*anchorname) {
				pfctl_clear_altq(dev, opts);
				pfctl_clear_states(dev, ifaceopt, opts);
				pfctl_clear_src_nodes(dev, opts);
				pfctl_clear_stats(dev, ifaceopt, opts);
				pfctl_clear_fingerprints(dev, opts);
				pfctl_clear_interface_flags(dev, opts);
			}
			break;
		case 'o':
			pfctl_clear_fingerprints(dev, opts);
			break;
		case 'T':
			pfctl_clear_tables(anchorname, opts);
			break;
		}
	}
	if (state_killers) {
		if (!strcmp(state_kill[0], "label"))
			pfctl_label_kill_states(dev, ifaceopt, opts);
		else if (!strcmp(state_kill[0], "id"))
			pfctl_id_kill_states(dev, ifaceopt, opts);
		else
			pfctl_net_kill_states(dev, ifaceopt, opts);
	}

	if (src_node_killers)
		pfctl_kill_src_nodes(dev, ifaceopt, opts);

	if (tblcmdopt != NULL) {
		error = pfctl_command_tables(argc, argv, tableopt,
		    tblcmdopt, rulesopt, anchorname, opts);
		rulesopt = NULL;
	}
	if (optiopt != NULL) {
		switch (*optiopt) {
		case 'n':
			optimize = 0;
			break;
		case 'b':
			optimize |= PF_OPTIMIZE_BASIC;
			break;
		case 'o':
		case 'p':
			optimize |= PF_OPTIMIZE_PROFILE;
			break;
		}
	}

	if ((rulesopt != NULL) && !anchorname[0])
		if (pfctl_clear_interface_flags(dev, opts | PF_OPT_QUIET))
			error = 1;

	if (rulesopt != NULL && !anchorname[0])
		if (pfctl_file_fingerprints(dev, opts, PF_OSFP_FILE))
			error = 1;

	if (rulesopt != NULL) {
		if (anchorname[0] == '_' || strstr(anchorname, "/_") != NULL)
			errx(1, "anchor names beginning with '_' cannot "
			    "be modified from the command line");
		if (pfctl_rules(dev, rulesopt, opts, optimize,
		    anchorname, NULL))
			error = 1;
		else if (!(opts & PF_OPT_NOACTION))
			warn_namespace_collision(NULL);
	}

	if (opts & PF_OPT_ENABLE)
		if (pfctl_enable(dev, opts))
			error = 1;

	if (debugopt != NULL) {
		if ((level = string_to_loglevel((char *)debugopt)) < 0) {
			switch (*debugopt) {
			case 'n':
				level = LOG_CRIT;
				break;
			case 'u':
				level = LOG_ERR;
				break;
			case 'm':
				level = LOG_NOTICE;
				break;
			case 'l':
				level = LOG_DEBUG;
				break;
			}
		}
		if (level >= 0)
			pfctl_debug(dev, level, opts);
	}

	if (sfile != NULL)
		pfctl_state_store(dev, sfile);
	if (lfile != NULL)
		pfctl_state_load(dev, lfile);

	exit(error);
}
