/*	$OpenBSD: pfctl.c,v 1.63 2002/05/19 21:09:56 deraadt Exp $ */

/*
 * Copyright (c) 2001 Daniel Hartmeier
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

#include <net/if.h>
#include <netinet/in.h>
#include <net/pfvar.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pfctl_parser.h"

void	 usage(void);
int	 pfctl_enable(int, int);
int	 pfctl_disable(int, int);
int	 pfctl_clear_stats(int, int);
int	 pfctl_clear_rules(int, int);
int	 pfctl_clear_nat(int, int);
int	 pfctl_clear_states(int, int);
int	 pfctl_kill_states(int, int);
int	 pfctl_hint(int, const char *, int);
int	 pfctl_show_rules(int, int, int);
int	 pfctl_show_nat(int);
int	 pfctl_show_states(int, u_int8_t, int);
int	 pfctl_show_status(int);
int	 pfctl_rules(int, char *, int);
int	 pfctl_nat(int, char *, int);
int	 pfctl_log(int, char *, int);
int	 pfctl_timeout(int, char *, int);
int	 pfctl_gettimeout(int, const char *);
int	 pfctl_settimeout(int, const char *, int);
int	 pfctl_limit(int, char *, int);
int	 pfctl_getlimit(int, const char *);
int	 pfctl_setlimit(int, const char *, unsigned);
int	 pfctl_debug(int, u_int32_t, int);
int	 pfctl_clear_rule_counters(int, int);

int	 opts = 0;
char	*clearopt;
char	*hintopt;
char	*logopt;
char	*natopt;
char	*rulesopt;
char	*showopt;
char	*timeoutopt;
char	*limitopt;
char	*debugopt;
int	 state_killers;
char 	*state_kill[2];

char	*infile;

static const struct {
	const char	*name;
	int		timeout;
} pf_timeouts[] = {
	{ "tcp.first",		PFTM_TCP_FIRST_PACKET },
	{ "tcp.opening",	PFTM_TCP_OPENING },
	{ "tcp.established",	PFTM_TCP_ESTABLISHED },
	{ "tcp.closing",	PFTM_TCP_CLOSING },
	{ "tcp.finwait",	PFTM_TCP_FIN_WAIT },
	{ "tcp.closed",		PFTM_TCP_CLOSED },
	{ "udp.first",		PFTM_UDP_FIRST_PACKET },
	{ "udp.single",		PFTM_UDP_SINGLE },
	{ "udp.multiple",	PFTM_UDP_MULTIPLE },
	{ "icmp.first",		PFTM_ICMP_FIRST_PACKET },
	{ "icmp.error",		PFTM_ICMP_ERROR_REPLY },
	{ "other.first",	PFTM_OTHER_FIRST_PACKET },
	{ "other.single",	PFTM_OTHER_SINGLE },
	{ "other.multiple",	PFTM_OTHER_MULTIPLE },
	{ "frag",		PFTM_FRAG },
	{ "interval",		PFTM_INTERVAL },
	{ NULL,			0 }
};

static const struct {
	const char	*name;
	int		index;
} pf_limits[] = {
	{ "states",	PF_LIMIT_STATES },
	{ "frags",	PF_LIMIT_FRAGS },
	{ NULL,		0 }
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
	{ NULL,			0 }
};
static const struct pf_hint pf_hint_satellite[] = {
	{ "tcp.first",		3 * 60 },
	{ "tcp.opening",	30 + 5 },
	{ "tcp.established",	24 * 60 * 60 },
	{ "tcp.closing",	15 * 60 + 5 },
	{ "tcp.finwait",	45 + 5 },
	{ "tcp.closed",		90 + 5 },
	{ NULL,			0 }
};
static const struct pf_hint pf_hint_conservative[] = {
	{ "tcp.first",		60 * 60 },
	{ "tcp.opening",	15 * 60 },
	{ "tcp.established",	5 * 24 * 60 * 60 },
	{ "tcp.closing",	60 * 60 },
	{ "tcp.finwait",	10 * 60 },
	{ "tcp.closed",		3 * 60 },
	{ NULL,			0 }
};
static const struct pf_hint pf_hint_aggressive[] = {
	{ "tcp.first",		30 },
	{ "tcp.opening",	5 },
	{ "tcp.established",	5 * 60 * 60 },
	{ "tcp.closing",	60 },
	{ "tcp.finwait",	30 },
	{ "tcp.closed",		30 },
	{ NULL,			0 }
};

static const struct {
	const char *name;
	const struct pf_hint *hint;
} pf_hints[] = {
	{ "normal",		pf_hint_normal },
	{ "default",		pf_hint_normal },
	{ "satellite",		pf_hint_satellite },
	{ "high-latency",	pf_hint_satellite },
	{ "conservative",	pf_hint_conservative },
	{ "aggressive",		pf_hint_aggressive },
	{ NULL,			NULL }
};

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-dehnqrvz] [-F modifier] ", __progname);
	fprintf(stderr, "[-N file] [-O level] [-R file] [-k host]\n");
	fprintf(stderr, "             ");
	fprintf(stderr, "[-l interface] [-m modifier] [-s modifier] ");
	fprintf(stderr, "[-t modifier] [-x level]\n");

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
		printf("pf enabled\n");
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
		printf("pf disabled\n");
	return (0);
}

int
pfctl_clear_stats(int dev, int opts)
{
	if (ioctl(dev, DIOCCLRSTATUS))
		err(1, "DIOCCLRSTATUS");
	if ((opts & PF_OPT_QUIET) == 0)
		printf("pf: statistics cleared\n");
	return (0);
}

int
pfctl_clear_rules(int dev, int opts)
{
	struct pfioc_rule pr;

	if (ioctl(dev, DIOCBEGINRULES, &pr.ticket))
		err(1, "DIOCBEGINRULES");
	else if (ioctl(dev, DIOCCOMMITRULES, &pr.ticket))
		err(1, "DIOCCOMMITRULES");
	if ((opts & PF_OPT_QUIET) == 0)
		printf("rules cleared\n");
	return (0);
}

int
pfctl_clear_nat(int dev, int opts)
{
	struct pfioc_nat pn;
	struct pfioc_binat pb;
	struct pfioc_rdr pr;

	if (ioctl(dev, DIOCBEGINNATS, &pn.ticket))
		err(1, "DIOCBEGINNATS");
	else if (ioctl(dev, DIOCCOMMITNATS, &pn.ticket))
		err(1, "DIOCCOMMITNATS");
	if (ioctl(dev, DIOCBEGINBINATS, &pb.ticket))
		err(1, "DIOCBEGINBINATS");
	else if (ioctl(dev, DIOCCOMMITBINATS, &pb.ticket))
		err(1, "DIOCCOMMITBINATS");
	else if (ioctl(dev, DIOCBEGINRDRS, &pr.ticket))
		err(1, "DIOCBEGINRDRS");
	else if (ioctl(dev, DIOCCOMMITRDRS, &pr.ticket))
		err(1, "DIOCCOMMITRDRS");
	if ((opts & PF_OPT_QUIET) == 0)
		printf("nat cleared\n");
	return (0);
}

int
pfctl_clear_states(int dev, int opts)
{
	if (ioctl(dev, DIOCCLRSTATES))
		err(1, "DIOCCLRSTATES");
	if ((opts & PF_OPT_QUIET) == 0)
		printf("states cleared\n");
	return (0);
}

int
pfctl_kill_states(int dev, int opts)
{
	struct pfioc_state_kill psk;
	struct addrinfo *res[2], *resp[2];
	struct sockaddr last_src, last_dst;
	int killed, sources, dests;
	int ret_ga;

	killed = sources = dests = 0;

	memset(&psk, 0, sizeof(psk));
	memset(&psk.psk_src.mask, 0xff, sizeof(psk.psk_src.mask));
	memset(&last_src, 0xff, sizeof(last_src));
	memset(&last_dst, 0xff, sizeof(last_dst));

	if ((ret_ga = getaddrinfo(state_kill[0], NULL, NULL, &res[0]))) {
		errx(1, "%s", gai_strerror(ret_ga));
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
			psk.psk_src.addr.addr.v4 =
			    ((struct sockaddr_in *)resp[0]->ai_addr)->sin_addr;
		else if (psk.psk_af == AF_INET6)
			psk.psk_src.addr.addr.v6 =
			    ((struct sockaddr_in6 *)resp[0]->ai_addr)->
			    sin6_addr;
		else 
			errx(1, "Unknown address family!?!?!");

		if (state_killers > 1) {
			dests = 0;
			memset(&psk.psk_dst.mask, 0xff,
			    sizeof(psk.psk_dst.mask));
			memset(&last_dst, 0xff, sizeof(last_dst));
			if ((ret_ga = getaddrinfo(state_kill[1], NULL, NULL,
			    &res[1]))) {
				errx(1, "%s", gai_strerror(ret_ga));
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
					psk.psk_dst.addr.addr.v4 =
					    ((struct sockaddr_in *)resp[1]->
					    ai_addr)->sin_addr;
				else if (psk.psk_af == AF_INET6)
					psk.psk_dst.addr.addr.v6 =
					    ((struct sockaddr_in6 *)resp[1]->
					    ai_addr)->sin6_addr;
				else 
					errx(1, "Unknown address family!?!?!");

				if (ioctl(dev, DIOCKILLSTATES, &psk))
					err(1, "DIOCKILLSTATES");
				killed += psk.psk_af;
				/* fixup psk.psk_af */
				psk.psk_af = resp[1]->ai_family;
			}
		} else {
			if (ioctl(dev, DIOCKILLSTATES, &psk))
				err(1, "DIOCKILLSTATES");
			killed += psk.psk_af;
			/* fixup psk.psk_af */
			psk.psk_af = res[0]->ai_family;
		}
	}

	freeaddrinfo(res[0]);
	if (res[1])
		freeaddrinfo(res[1]);

	if ((opts & PF_OPT_QUIET) == 0)
		printf("killed %d states from %d sources and %d destinations\n",
		    killed, sources, dests);
	return (0);
}

int
pfctl_show_rules(int dev, int opts, int format)
{
	struct pfioc_rule pr;
	u_int32_t nr, mnr;

	if (ioctl(dev, DIOCGETRULES, &pr)) {
		warnx("DIOCGETRULES");
		return (-1);
	}
	mnr = pr.nr;
	for (nr = 0; nr < mnr; ++nr) {
		pr.nr = nr;
		if (ioctl(dev, DIOCGETRULE, &pr)) {
			warnx("DIOCGETRULE");
			return (-1);
		}
		switch (format) {
		case 1:
			if (pr.rule.label[0]) {
				if (opts & PF_OPT_VERBOSE)
					print_rule(&pr.rule);
				else
					printf("%s ", pr.rule.label);
				printf("%llu %llu %llu\n",
				    pr.rule.evaluations, pr.rule.packets,
				    pr.rule.bytes);
			}
			break;
		default:
			print_rule(&pr.rule);
			if (opts & PF_OPT_VERBOSE)
				printf("[ Evaluations: %-10llu  Packets: %-10llu  "
				    "Bytes: %-10llu ]\n\n", pr.rule.evaluations,
				    pr.rule.packets, pr.rule.bytes);
		}
	}
	return (0);
}

int
pfctl_show_nat(int dev)
{
	struct pfioc_nat pn;
	struct pfioc_rdr pr;
	struct pfioc_binat pb;
	u_int32_t mnr, nr;

	if (ioctl(dev, DIOCGETNATS, &pn)) {
		warnx("DIOCGETNATS");
		return (-1);
	}
	mnr = pn.nr;
	for (nr = 0; nr < mnr; ++nr) {
		pn.nr = nr;
		if (ioctl(dev, DIOCGETNAT, &pn)) {
			warnx("DIOCGETNAT");
			return (-1);
		}
		print_nat(&pn.nat);
	}
	if (ioctl(dev, DIOCGETRDRS, &pr)) {
		warnx("DIOCGETRDRS");
		return (-1);
	}
	mnr = pr.nr;
	for (nr = 0; nr < mnr; ++nr) {
		pr.nr = nr;
		if (ioctl(dev, DIOCGETRDR, &pr)) {
			warnx("DIOCGETRDR");
			return (-1);
		}
		print_rdr(&pr.rdr);
	}
	if (ioctl(dev, DIOCGETBINATS, &pb)) {
		warnx("DIOCGETBINATS");
		return (-1);
	}
	mnr = pb.nr;
	for (nr = 0; nr < mnr; ++nr) {
		pb.nr = nr;
		if (ioctl(dev, DIOCGETBINAT, &pb)) {
			warnx("DIOCGETBINAT");
			return (-1);
		}
		print_binat(&pb.binat);
	}
	return (0);
}

int
pfctl_show_states(int dev, u_int8_t proto, int opts)
{
	struct pfioc_states ps;
	struct pf_state *p;
	char *inbuf = NULL;
	int i, len = 0;

	for (;;) {
		ps.ps_len = len;
		if (len) {
			ps.ps_buf = inbuf = realloc(inbuf, len);
			if (inbuf == NULL)
				err(1, "malloc");
		}
		if (ioctl(dev, DIOCGETSTATES, &ps) < 0) {
			warnx("DIOCGETSTATES");
			return (-1);
		}
		if (ps.ps_len + sizeof(struct pfioc_state) < len)
			break;
		if (len == 0 && ps.ps_len == 0)
			return (0);
		if (len == 0 && ps.ps_len != 0)
			len = ps.ps_len;
		if (ps.ps_len == 0)
			return (0);	/* no states */
		len *= 2;
	}
	p = ps.ps_states;
	for (i = 0; i < ps.ps_len; i += sizeof(*p)) {
		if (!proto || (p->proto == proto))
			print_state(p, opts);
		p++;
	}
	return (0);
}

int
pfctl_show_status(int dev)
{
	struct pf_status status;

	if (ioctl(dev, DIOCGETSTATUS, &status)) {
		warnx("DIOCGETSTATUS");
		return (-1);
	}
	print_status(&status);
	return (0);
}

/* callbacks for rule/nat/rdr */

int
pfctl_add_rule(struct pfctl *pf, struct pf_rule *r)
{
	memcpy(&pf->prule->rule, r, sizeof(pf->prule->rule));
	if ((pf->opts & PF_OPT_NOACTION) == 0) {
		if (ioctl(pf->dev, DIOCADDRULE, pf->prule))
			err(1, "DIOCADDRULE");
	}
	if (pf->opts & PF_OPT_VERBOSE)
		print_rule(&pf->prule->rule);
	return (0);
}

int
pfctl_add_nat(struct pfctl *pf, struct pf_nat *n)
{
	memcpy(&pf->pnat->nat, n, sizeof(pf->pnat->nat));
	if ((pf->opts & PF_OPT_NOACTION) == 0) {
		if (ioctl(pf->dev, DIOCADDNAT, pf->pnat))
			err(1, "DIOCADDNAT");
	}
	if (pf->opts & PF_OPT_VERBOSE)
		print_nat(&pf->pnat->nat);
	return (0);
}

int
pfctl_add_binat(struct pfctl *pf, struct pf_binat *b)
{
	memcpy(&pf->pbinat->binat, b, sizeof(pf->pbinat->binat));
	if ((pf->opts & PF_OPT_NOACTION) == 0) {
		if (ioctl(pf->dev, DIOCADDBINAT, pf->pbinat))
			err(1, "DIOCADDBINAT");
	}
	if (pf->opts & PF_OPT_VERBOSE)
		print_binat(&pf->pbinat->binat);
	return (0);
}

int
pfctl_add_rdr(struct pfctl *pf, struct pf_rdr *r)
{
	memcpy(&pf->prdr->rdr, r, sizeof(pf->prdr->rdr));
	if ((pf->opts & PF_OPT_NOACTION) == 0) {
		if (ioctl(pf->dev, DIOCADDRDR, pf->prdr))
			err(1, "DIOCADDRDR");
	}
	if (pf->opts & PF_OPT_VERBOSE)
		print_rdr(&pf->prdr->rdr);
	return (0);
}

int
pfctl_rules(int dev, char *filename, int opts)
{
	FILE *fin;
	struct pfioc_rule	pr;
	struct pfctl		pf;

	if (strcmp(filename, "-") == 0) {
		infile = "stdin";
		fin = stdin;
	} else {
		fin = fopen(filename, "r");
		infile = filename;
	}
	if (fin == NULL) {
		warn("%s", filename);
		return (1);
	}
	if ((opts & PF_OPT_NOACTION) == 0) {
		if (ioctl(dev, DIOCBEGINRULES, &pr.ticket))
			err(1, "DIOCBEGINRULES");
	}
	/* fill in callback data */
	pf.dev = dev;
	pf.opts = opts;
	pf.prule = &pr;
	pf.rule_nr = 0;
	if (parse_rules(fin, &pf) < 0)
		errx(1, "syntax error in rule file: pf rules not loaded");
	if ((opts & PF_OPT_NOACTION) == 0) {
		if (ioctl(dev, DIOCCOMMITRULES, &pr.ticket))
			err(1, "DIOCCOMMITRULES");
#if 0
		if ((opts & PF_OPT_QUIET) == 0)
			printf("%u rules loaded\n", n);
#endif
	}
	if (fin != stdin)
		fclose(fin);
	return (0);
}

int
pfctl_nat(int dev, char *filename, int opts)
{
	FILE *fin;
	struct pfioc_nat	pn;
	struct pfioc_binat	pb;
	struct pfioc_rdr	pr;
	struct pfctl		pf;

	if (strcmp(filename, "-") == 0) {
		fin = stdin;
		infile = "stdin";
	} else {
		fin = fopen(filename, "r");
		infile = filename;
	}
	if (fin == NULL) {
		warn("%s", filename);
		return (1);
	}

	if ((opts & PF_OPT_NOACTION) == 0) {
		if (ioctl(dev, DIOCBEGINNATS, &pn.ticket))
			err(1, "DIOCBEGINNATS");
		if (ioctl(dev, DIOCBEGINRDRS, &pr.ticket))
			err(1, "DIOCBEGINRDRS");
		if (ioctl(dev, DIOCBEGINBINATS, &pb.ticket))
			err(1, "DIOCBEGINBINATS");
	}
	/* fill in callback data */
	pf.dev = dev;
	pf.opts = opts;
	pf.pnat = &pn;
	pf.pbinat = &pb;
	pf.prdr = &pr;
	if (parse_nat(fin, &pf) < 0)
		errx(1, "syntax error in file: nat rules not loaded");
	if ((opts & PF_OPT_NOACTION) == 0) {
		if (ioctl(dev, DIOCCOMMITNATS, &pn.ticket))
			err(1, "DIOCCOMMITNATS");
		if (ioctl(dev, DIOCCOMMITRDRS, &pr.ticket))
			err(1, "DIOCCOMMITRDRS");
		if (ioctl(dev, DIOCCOMMITBINATS, &pb.ticket))
			err(1, "DIOCCOMMITBINATS");
#if 0
		if ((opts & PF_OPT_QUIET) == 0) {
			printf("%u nat entries loaded\n", n);
			printf("%u rdr entries loaded\n", r);
			printf("%u binat entries loaded\n", b);
		}
#endif
	}
	if (fin != stdin)
		fclose(fin);
	return (0);
}

int
pfctl_log(int dev, char *ifname, int opts)
{
	struct pfioc_if pi;

	strlcpy(pi.ifname, ifname, sizeof(pi.ifname));
	if (ioctl(dev, DIOCSETSTATUSIF, &pi))
		err(1, "DIOCSETSTATUSIF");
	if ((opts & PF_OPT_QUIET) == 0)
		printf("now logging %s\n", pi.ifname);
	return (0);
}

int
pfctl_hint(int dev, const char *opt, int opts)
{
	const struct pf_hint *hint;
	int i, r;

	for (i = 0; pf_hints[i].name; i++)
		if (strcasecmp(opt, pf_hints[i].name) == 0)
			break;

	hint = pf_hints[i].hint;
	if (hint == NULL) {
		warnx("Bad hint name.  Format -O hint");
		return (1);
	}

	for (i = 0; hint[i].name; i++)
		if ((r = pfctl_settimeout(dev, hint[i].name, hint[i].timeout)))
			return (r);
	return (0);
}

int
pfctl_limit(int dev, char *opt, int opts)
{
	char *arg, *serr = NULL;
	unsigned limit;

	arg = index(opt, '=');
	if (arg == NULL)
		return pfctl_getlimit(dev, opt);
	else {
		if (*arg)
			*arg++ = 0;
		if (strcasecmp(arg, "inf") == 0)
			limit = UINT_MAX;
		else {
			limit = strtol(arg, &serr, 10);
			if (*serr || !*arg) {
				warnx("Bad limit argument.  "
				    "Format -m name=limit");
				return (1);
			}
		}
		return pfctl_setlimit(dev, opt, limit);
	}
}

int
pfctl_getlimit(int dev, const char *opt)
{
	struct pfioc_limit pl;
	int i, found = 0;

	for (i = 0; pf_limits[i].name; i++) {
		if (strcmp(opt, "all") == 0 ||
		    strcasecmp(opt, pf_limits[i].name) == 0) {
			found = 1;
			pl.index = i;
			if (ioctl(dev, DIOCGETLIMIT, &pl))
				err(1, "DIOCGETLIMIT");
			printf("%-10s ", pf_limits[i].name);
			if (pl.limit == UINT_MAX)
				printf("unlimited\n");
			else
				printf("hard limit %6u\n", pl.limit);
		}
	}
	if (found == 0) {
		warnx("Bad pool name.  Format -m name[=<limit>]");
		return (1);
	}
	return (0);
}

int
pfctl_setlimit(int dev, const char *opt, unsigned limit)
{
	struct pfioc_limit pl;
	int i;

	for (i = 0; pf_limits[i].name; i++) {
		if (strcasecmp(opt, pf_limits[i].name) == 0) {
			pl.index = i;
			pl.limit = limit;
			if (ioctl(dev, DIOCSETLIMIT, &pl)) {
				if (errno == EBUSY) {
					warnx("Current pool size exceeds "
					    "requested hard limit");
					return (1);
				} else
					err(1, "DIOCSETLIMIT");
			}
			if ((opts & PF_OPT_QUIET) == 0) {
				printf("%s ", pf_limits[i].name);
				if (pl.limit == UINT_MAX)
					printf("unlimited");
				else
					printf("hard limit %u", pl.limit);
				printf(" -> ");
				if (limit == UINT_MAX)
					printf("unlimited");
				else
					printf("hard limit %u", limit);
				printf("\n");
			}
			break;
		}
	}
	if (pf_limits[i].name == NULL) {
		warnx("Bad pool name.  Format -m name[=<limit>]");
		return (1);
	}
	return (0);
}

int
pfctl_timeout(int dev, char *opt, int opts)
{
	char *seconds, *serr = NULL;
	int setval;

	seconds = index(opt, '=');
	if (seconds == NULL)
		return pfctl_gettimeout(dev, opt);
	else {
		/* Set the timeout value */
		if (*seconds != '\0')
			*seconds++ = '\0';	/* Eat '=' */
		setval = strtol(seconds, &serr, 10);
		if (*serr != '\0' || *seconds == '\0' || setval < 0) {
			warnx("Bad timeout argument.  Format -t name=seconds");
			return (1);
		}
		return pfctl_settimeout(dev, opt, setval);
	}
}

int
pfctl_gettimeout(int dev, const char *opt)
{
	struct pfioc_tm pt;
	int i;

	for (i = 0; pf_timeouts[i].name; i++) {
		if (strcmp(opt, "all") == 0) {
			/* Need to dump all of the values */
			pt.timeout = pf_timeouts[i].timeout;
			if (ioctl(dev, DIOCGETTIMEOUT, &pt))
				err(1, "DIOCGETTIMEOUT");
			printf("%-20s %ds\n", pf_timeouts[i].name, pt.seconds);
		} else if (strcasecmp(opt, pf_timeouts[i].name) == 0) {
			pt.timeout = pf_timeouts[i].timeout;
			break;
		}
	}
	if (strcmp(opt, "all") == 0)
		return (0);

	if (pf_timeouts[i].name == NULL) {
		warnx("Bad timeout name.  Format -t name[=<seconds>]");
		return (1);
	}

	if (ioctl(dev, DIOCGETTIMEOUT, &pt))
		err(1, "DIOCSETTIMEOUT");
	if ((opts & PF_OPT_QUIET) == 0)
		printf("%s timeout %ds\n", pf_timeouts[i].name,
		    pt.seconds);
	return (0);
}

int
pfctl_settimeout(int dev, const char *opt, int seconds)
{
	struct pfioc_tm pt;
	int i;

	for (i = 0; pf_timeouts[i].name; i++) {
		if (strcasecmp(opt, pf_timeouts[i].name) == 0) {
			pt.timeout = pf_timeouts[i].timeout;
			break;
		}
	}

	if (pf_timeouts[i].name == NULL) {
		warnx("Bad timeout name.  Format -t name[=<seconds>]");
		return (1);
	}

	pt.seconds = seconds;
	if (ioctl(dev, DIOCSETTIMEOUT, &pt))
		err(1, "DIOCSETTIMEOUT");
	if ((opts & PF_OPT_QUIET) == 0)
		printf("%s timeout %ds -> %ds\n", pf_timeouts[i].name,
		    pt.seconds, seconds);
	return (0);
}

int
pfctl_debug(int dev, u_int32_t level, int opts)
{
	if (ioctl(dev, DIOCSETDEBUG, &level))
		err(1, "DIOCSETDEBUG");
	if ((opts & PF_OPT_QUIET) == 0) {
		printf("debug level set to '");
		switch (level) {
			case PF_DEBUG_NONE:
				printf("none");
				break;
			case PF_DEBUG_URGENT:
				printf("urgent");
				break;
			case PF_DEBUG_MISC:
				printf("misc");
				break;
			default:
				printf("<invalid>");
				break;
		}
		printf("'\n");
	}
	return (0);
}

int
pfctl_clear_rule_counters(int dev, int opts)
{
	if (ioctl(dev, DIOCCLRRULECTRS))
		err(1, "DIOCCLRRULECTRS");
	if ((opts & PF_OPT_QUIET) == 0)
		printf("pf: rule counters cleared\n");
	return (0);
}

int
main(int argc, char *argv[])
{
	int error = 0;
	int dev = -1;
	int ch;
	int mode = O_RDONLY;

	if (argc < 2)
		usage();

	while ((ch = getopt(argc, argv, "deqF:hk:l:m:nN:O:rR:s:t:vx:z")) != -1) {
		switch (ch) {
		case 'd':
			opts |= PF_OPT_DISABLE;
			mode = O_RDWR;
			break;
		case 'e':
			opts |= PF_OPT_ENABLE;
			mode = O_RDWR;
			break;
		case 'q':
			opts |= PF_OPT_QUIET;
			break;
		case 'F':
			clearopt = optarg;
			mode = O_RDWR;
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
		case 'l':
			logopt = optarg;
			mode = O_RDWR;
			break;
		case 'm':
			limitopt = optarg;
			if (strchr(limitopt, '=') != NULL)
				mode = O_RDWR;
			break;
		case 'n':
			opts |= PF_OPT_NOACTION;
			break;
		case 'N':
			natopt = optarg;
			mode = O_RDWR;
			break;
		case 'O':
			hintopt = optarg;
			mode = O_RDWR;
			break;
		case 'r':
			opts |= PF_OPT_USEDNS;
			break;
		case 'R':
			rulesopt = optarg;
			mode = O_RDWR;
			break;
		case 's':
			showopt = optarg;
			break;
		case 't':
			timeoutopt = optarg;
			if (strchr(timeoutopt, '=') != NULL)
				mode = O_RDWR;
			break;
		case 'v':
			opts |= PF_OPT_VERBOSE;
			break;
		case 'x':
			debugopt = optarg;
			mode = O_RDWR;
			break;
		case 'z':
			opts |= PF_OPT_CLRRULECTRS;
			mode = O_RDWR;
			break;
		case 'h':
			/* FALLTHROUGH */
		default:
			usage();
			/* NOTREACHED */
		}
	}

	if (argc != optind) {
		warnx("unknown command line argument: %s ...", argv[optind]);
		usage();
		/* NOTREACHED */
	}

	if (opts & PF_OPT_NOACTION)
		mode = O_RDONLY;
	if ((opts & PF_OPT_NOACTION) == 0) {
		dev = open("/dev/pf", mode);
		if (dev == -1)
			err(1, "open(\"/dev/pf\")");
	} else {
		/* turn off options */
		opts &= ~ (PF_OPT_DISABLE | PF_OPT_ENABLE);
		clearopt = logopt = showopt = debugopt = NULL;
	}

	if (opts & PF_OPT_DISABLE)
		if (pfctl_disable(dev, opts))
			error = 1;

	if (clearopt != NULL) {
		switch (*clearopt) {
		case 'r':
			pfctl_clear_rules(dev, opts);
			break;
		case 'n':
			pfctl_clear_nat(dev, opts);
			break;
		case 's':
			pfctl_clear_states(dev, opts);
			break;
		case 'i':
			pfctl_clear_stats(dev, opts);
			break;
		case 'a':
			pfctl_clear_rules(dev, opts);
			pfctl_clear_nat(dev, opts);
			pfctl_clear_states(dev, opts);
			pfctl_clear_stats(dev, opts);
			break;
		default:
			warnx("Unknown flush modifier '%s'", clearopt);
			error = 1;
		}
	}
	if (state_killers)
		pfctl_kill_states(dev, opts);

	if (rulesopt != NULL)
		if (pfctl_rules(dev, rulesopt, opts))
			error = 1;

	if (natopt != NULL)
		if (pfctl_nat(dev, natopt, opts))
			error = 1;

	if (showopt != NULL) {
		switch (*showopt) {
		case 'r':
			pfctl_show_rules(dev, opts, 0);
			break;
		case 'l':
			pfctl_show_rules(dev, opts, 1);
			break;
		case 'n':
			pfctl_show_nat(dev);
			break;
		case 's':
			pfctl_show_states(dev, 0, opts);
			break;
		case 'i':
			pfctl_show_status(dev);
			break;
		case 'a':
			pfctl_show_rules(dev, opts, 0);
			pfctl_show_nat(dev);
			pfctl_show_states(dev, 0, opts);
			pfctl_show_status(dev);
			break;
		default:
			warnx("Unknown show modifier '%s'", showopt);
			error = 1;
		}
	}

	if (logopt != NULL)
		if (pfctl_log(dev, logopt, opts))
			error = 1;

	if (hintopt != NULL)
		if (pfctl_hint(dev, hintopt, opts))
			error = 1;

	if (timeoutopt != NULL)
		if (pfctl_timeout(dev, timeoutopt, opts))
			error = 1;

	if (limitopt != NULL)
		if (pfctl_limit(dev, limitopt, opts))
			error = 1;

	if (opts & PF_OPT_ENABLE)
		if (pfctl_enable(dev, opts))
			error = 1;

	if (debugopt != NULL) {
		switch (*debugopt) {
		case 'n':
			pfctl_debug(dev, PF_DEBUG_NONE, opts);
			break;
		case 'u':
			pfctl_debug(dev, PF_DEBUG_URGENT, opts);
			break;
		case 'm':
			pfctl_debug(dev, PF_DEBUG_MISC, opts);
			break;
		default:
			warnx("Unknown debug level '%s'", debugopt);
			error = 1;
		}
	}

	if (opts & PF_OPT_CLRRULECTRS) {
		if (pfctl_clear_rule_counters(dev, opts))
			error = 1;
	}
	close(dev);
	exit(error);
}
