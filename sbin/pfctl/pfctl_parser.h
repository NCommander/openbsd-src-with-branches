/*	$OpenBSD: pfctl_parser.h,v 1.60 2003/05/10 00:45:24 henning Exp $ */

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

#ifndef _PFCTL_PARSER_H_
#define _PFCTL_PARSER_H_

#define PF_OPT_DISABLE		0x0001
#define PF_OPT_ENABLE		0x0002
#define PF_OPT_VERBOSE		0x0004
#define PF_OPT_NOACTION		0x0008
#define PF_OPT_QUIET		0x0010
#define PF_OPT_CLRRULECTRS	0x0020
#define PF_OPT_USEDNS		0x0040
#define PF_OPT_VERBOSE2		0x0080
#define PF_OPT_DUMMYACTION	0x0100
#define PF_OPT_DEBUG		0x0200

#define PF_TH_ALL		0xFF

#define PF_NAT_PROXY_PORT_LOW	50001
#define PF_NAT_PROXY_PORT_HIGH	65535

#define FCNT_NAMES { \
	"searches", \
	"inserts", \
	"removals", \
	NULL \
}

struct pfctl {
	int dev;
	int opts;
	int loadopt;
	u_int32_t rule_nr;
	struct pfioc_pooladdr paddr;
	struct pfioc_rule *prule[PF_RULESET_MAX];
	struct pfioc_altq *paltq;
	struct pfioc_queue *pqueue;
	const char *anchor;
	const char *ruleset;
};

enum pfctl_iflookup_mode {
	PFCTL_IFLOOKUP_HOST,
	PFCTL_IFLOOKUP_NET,
	PFCTL_IFLOOKUP_BCAST
};

struct node_if {
	char			 ifname[IFNAMSIZ];
	u_int8_t		 not;
	u_int			 ifa_flags;
	struct node_if		*next;
	struct node_if		*tail;
};

struct node_host {
	struct pf_addr_wrap	 addr;
	struct pf_addr		 bcast;
	sa_family_t		 af;
	u_int8_t		 not;
	u_int32_t		 ifindex;	/* link-local IPv6 addrs */
	char			*ifname;
	u_int			 ifa_flags;
	struct node_host	*next;
	struct node_host	*tail;
};

struct node_queue_bw {
	u_int32_t	bw_absolute;
	u_int16_t	bw_percent;
};

struct node_hfsc_sc {
	struct node_queue_bw	m1;	/* slope of 1st segment; bps */
	u_int			d;	/* x-projection of m1; msec */
	struct node_queue_bw	m2;	/* slope of 2nd segment; bps */
	u_int8_t		used;
};

struct node_hfsc_opts {
	struct node_hfsc_sc	realtime;
	struct node_hfsc_sc	linkshare;
	struct node_hfsc_sc	upperlimit;
	int			flags;
};

struct node_queue_opt {
	int			 qtype;
	union {
		struct cbq_opts		cbq_opts;
		struct priq_opts	priq_opts;
		struct node_hfsc_opts	hfsc_opts;
	}			 data;
};

int	pfctl_rules(int, char *, int, char *, char *);

int	pfctl_add_rule(struct pfctl *, struct pf_rule *);
int	pfctl_add_altq(struct pfctl *, struct pf_altq *);
int	pfctl_add_pool(struct pfctl *, struct pf_pool *, sa_family_t);
void	pfctl_clear_pool(struct pf_pool *);

int	pfctl_set_timeout(struct pfctl *, const char *, int, int);
int	pfctl_set_optimization(struct pfctl *, const char *);
int	pfctl_set_limit(struct pfctl *, const char *, unsigned int);
int	pfctl_set_logif(struct pfctl *, char *);

int	parse_rules(FILE *, struct pfctl *);
int	parse_flags(char *);
int	pfctl_load_anchors(int, int);

void	print_pool(struct pf_pool *, u_int16_t, u_int16_t, sa_family_t, int);
void	print_rule(struct pf_rule *, int);
void	print_status(struct pf_status *);

int	eval_pfaltq(struct pfctl *, struct pf_altq *, struct node_queue_bw *,
	    struct node_queue_opt *);
int	eval_pfqueue(struct pfctl *, struct pf_altq *, struct node_queue_bw *,
	    struct node_queue_opt *);

void	 print_altq(const struct pf_altq *, unsigned, struct node_queue_bw *,
	     struct node_queue_opt *);
void	 print_queue(const struct pf_altq *, unsigned, struct node_queue_bw *,
	     int, struct node_queue_opt *);

void	pfctl_begin_table(void);
void	pfctl_append_addr(char *, int, int);
void	pfctl_append_file(char *);
void	pfctl_define_table(char *, int, int, int, const char *, const char *);
void	pfctl_commit_table(void);

struct icmptypeent {
	const char *name;
	u_int8_t type;
};

struct icmpcodeent {
	const char *name;
	u_int8_t type;
	u_int8_t code;
};

const struct icmptypeent *geticmptypebynumber(u_int8_t, u_int8_t);
const struct icmptypeent *geticmptypebyname(char *, u_int8_t);
const struct icmpcodeent *geticmpcodebynumber(u_int8_t, u_int8_t, u_int8_t);
const struct icmpcodeent *geticmpcodebyname(u_long, char *, u_int8_t);

struct pf_timeout {
	const char	*name;
	int		 timeout;
};

#define PFCTL_FLAG_ALL		0x01
#define PFCTL_FLAG_FILTER	0x02
#define PFCTL_FLAG_NAT		0x04
#define PFCTL_FLAG_OPTION	0x08
#define PFCTL_FLAG_ALTQ		0x10
#define PFCTL_FLAG_TABLE	0x20

extern const struct pf_timeout pf_timeouts[];

void			 set_ipmask(struct node_host *, u_int8_t);
int			 check_netmask(struct node_host *, sa_family_t);
void			 ifa_load(void);
struct node_host	*ifa_exists(const char *);
struct node_host	*ifa_lookup(const char *, enum pfctl_iflookup_mode);
struct node_host	*host(const char *);

#endif /* _PFCTL_PARSER_H_ */
