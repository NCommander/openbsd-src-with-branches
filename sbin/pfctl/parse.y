/*	$OpenBSD: parse.y,v 1.346 2003/03/27 16:11:55 henning Exp $	*/

/*
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
 * Copyright (c) 2001 Daniel Hartmeier.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
%{
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <net/pfvar.h>
#include <arpa/inet.h>
#include <altq/altq.h>
#include <altq/altq_cbq.h>
#include <altq/altq_priq.h>
#include <altq/altq_hfsc.h>

#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <err.h>
#include <pwd.h>
#include <grp.h>
#include <md5.h>

#include "pfctl_parser.h"
#include "pfctl.h"

static struct pfctl	*pf = NULL;
static FILE		*fin = NULL;
static int		 debug = 0;
static int		 lineno = 1;
static int		 errors = 0;
static int		 rulestate = 0;
static u_int16_t	 returnicmpdefault =
			    (ICMP_UNREACH << 8) | ICMP_UNREACH_PORT;
static u_int16_t	 returnicmp6default =
			    (ICMP6_DST_UNREACH << 8) | ICMP6_DST_UNREACH_NOPORT;
static int		 blockpolicy = PFRULE_DROP;
static int		 require_order = 1;

enum {
	PFCTL_STATE_NONE,
	PFCTL_STATE_OPTION,
	PFCTL_STATE_SCRUB,
	PFCTL_STATE_QUEUE,
	PFCTL_STATE_NAT,
	PFCTL_STATE_FILTER
};

struct node_proto {
	u_int8_t		 proto;
	struct node_proto	*next;
	struct node_proto	*tail;
};

struct node_port {
	u_int16_t		 port[2];
	u_int8_t		 op;
	struct node_port	*next;
	struct node_port	*tail;
};

struct node_uid {
	uid_t			 uid[2];
	u_int8_t		 op;
	struct node_uid		*next;
	struct node_uid		*tail;
};

struct node_gid {
	gid_t			 gid[2];
	u_int8_t		 op;
	struct node_gid		*next;
	struct node_gid		*tail;
};

struct node_icmp {
	u_int8_t		 code;
	u_int8_t		 type;
	u_int8_t		 proto;
	struct node_icmp	*next;
	struct node_icmp	*tail;
};

enum	{ PF_STATE_OPT_MAX=0, PF_STATE_OPT_TIMEOUT=1 };
struct node_state_opt {
	int			 type;
	union {
		u_int32_t	 max_states;
		struct {
			int		number;
			u_int32_t	seconds;
		}		 timeout;
	}			 data;
	struct node_state_opt	*next;
	struct node_state_opt	*tail;
};

struct peer {
	struct node_host	*host;
	struct node_port	*port;
};

struct node_queue {
	char			 queue[PF_QNAME_SIZE];
	char			 parent[PF_QNAME_SIZE];
	char			 ifname[IFNAMSIZ];
	int			 scheduler;
	struct node_queue	*next;
	struct node_queue	*tail;
}	*queues = NULL;

struct node_queue_opt {
	int			 qtype;
	union {
		struct cbq_opts		cbq_opts;
		struct priq_opts	priq_opts;
	}			 data;
};

struct node_queue_bw {
	u_int32_t	bw_absolute;
	u_int16_t	bw_percent;
};

struct node_qassign {
	char		*qname;
	char		*pqname;
};

struct filter_opts {
	int			 marker;
#define FOM_FLAGS	0x01
#define FOM_ICMP	0x02
#define FOM_TOS		0x04
#define FOM_KEEP	0x08
	struct node_uid		*uid;
	struct node_gid		*gid;
	struct {
		u_int8_t	 b1;
		u_int8_t	 b2;
		u_int16_t	 w;
		u_int16_t	 w2;
	} flags;
	struct node_icmp	*icmpspec;
	u_int32_t		 tos;
	struct {
		int			 action;
		struct node_state_opt	*options;
	} keep;
	int			 fragment;
	int			 allowopts;
	char			*label;
	struct node_qassign	 queues;
} filter_opts;

struct scrub_opts {
	int			marker;
#define SOM_MINTTL	0x01
#define SOM_MAXMSS	0x02
#define SOM_FRAGCACHE	0x04
	int			nodf;
	int			minttl;
	int			maxmss;
	int			fragcache;
	int			randomid;
} scrub_opts;

struct queue_opts {
	int			marker;
#define QOM_BWSPEC	0x01
#define QOM_SCHEDULER	0x02
#define QOM_PRIORITY	0x04
#define QOM_TBRSIZE	0x08
#define QOM_QLIMIT	0x10
	struct node_queue_bw	queue_bwspec;
	struct node_queue_opt	scheduler;
	int			priority;
	int			tbrsize;
	int			qlimit;
} queue_opts;

struct table_opts {
	int			flags;
	int			init_addr;
} table_opts;

int	yyerror(char *, ...);
int	disallow_table(struct node_host *, char *);
int	rule_consistent(struct pf_rule *);
int	filter_consistent(struct pf_rule *);
int	nat_consistent(struct pf_rule *);
int	rdr_consistent(struct pf_rule *);
int	yyparse(void);
void	expand_label_str(char *, const char *, const char *);
void	expand_label_if(const char *, char *, const char *);
void	expand_label_addr(const char *, char *, u_int8_t, struct node_host *);
void	expand_label_port(const char *, char *, struct node_port *);
void	expand_label_proto(const char *, char *, u_int8_t);
void	expand_label_nr(const char *, char *);
void	expand_label(char *, const char *, u_int8_t, struct node_host *,
	    struct node_port *, struct node_host *, struct node_port *,
	    u_int8_t);
void	expand_rule(struct pf_rule *, struct node_if *, struct node_host *,
	    struct node_proto *, struct node_host *, struct node_port *,
	    struct node_host *, struct node_port *, struct node_uid *,
	    struct node_gid *, struct node_icmp *);
int	expand_altq(struct pf_altq *, struct node_if *, struct node_queue *,
	    struct node_queue_bw bwspec);
int	expand_queue(struct pf_altq *, struct node_queue *,
	    struct node_queue_bw);

int	 check_rulestate(int);
int	 kw_cmp(const void *, const void *);
int	 lookup(char *);
int	 lgetc(FILE *);
int	 lungetc(int);
int	 findeol(void);
int	 yylex(void);
int	 atoul(char *, u_long *);
int	 getservice(char *);

TAILQ_HEAD(symhead, sym)	 symhead = TAILQ_HEAD_INITIALIZER(symhead);
struct sym {
	TAILQ_ENTRY(sym)	 entries;
	int			 used;
	int			 persist;
	char			*nam;
	char			*val;
};


int	 symset(const char *, const char *, int);
char	*symget(const char *);

void	 decide_address_family(struct node_host *, sa_family_t *);
void	 remove_invalid_hosts(struct node_host **, sa_family_t *);
int	 invalid_redirect(struct node_host *, sa_family_t);
u_int16_t parseicmpspec(char *, sa_family_t);

typedef struct {
	union {
		u_int32_t		 number;
		int			 i;
		char			*string;
		struct {
			u_int8_t	 b1;
			u_int8_t	 b2;
			u_int16_t	 w;
			u_int16_t	 w2;
		}			 b;
		struct range {
			int		 a;
			int		 b;
			int		 t;
		}			 range;
		struct node_if		*interface;
		struct node_proto	*proto;
		struct node_icmp	*icmp;
		struct node_host	*host;
		struct node_port	*port;
		struct node_uid		*uid;
		struct node_gid		*gid;
		struct node_state_opt	*state_opt;
		struct peer		 peer;
		struct {
			struct peer	 src, dst;
		}			 fromto;
		struct pf_poolhashkey	*hashkey;
		struct {
			struct node_host	*host;
			u_int8_t		 rt;
			u_int8_t		 pool_opts;
			sa_family_t		 af;
			struct pf_poolhashkey	*key;
		}			 route;
		struct redirection {
			struct node_host	*host;
			struct range		 rport;
		}			*redirection;
		struct {
			int			 type;
			struct pf_poolhashkey	*key;
		}			 pooltype;
		struct {
			int			 action;
			struct node_state_opt	*options;
		}			 keep_state;
		struct {
			u_int8_t	 log;
			u_int8_t	 quick;
		}			 logquick;
		struct node_queue	*queue;
		struct node_queue_opt	 queue_options;
		struct node_queue_bw	 queue_bwspec;
		struct node_qassign	 qassign;
		struct filter_opts	 filter_opts;
		struct queue_opts	 queue_opts;
		struct scrub_opts	 scrub_opts;
		struct table_opts	 table_opts;
	} v;
	int lineno;
} YYSTYPE;

#define PREPARE_ANCHOR_RULE(r, a)				\
	do {							\
		memset(&(r), 0, sizeof(r));			\
		if (strlcpy(r.anchorname, (a),			\
		    sizeof(r.anchorname)) >=			\
		    sizeof(r.anchorname)) {			\
			yyerror("anchor name '%s' too long",	\
			    (a));				\
			YYERROR;				\
		}						\
	} while (0)

%}

%token	PASS BLOCK SCRUB RETURN IN OUT LOG LOGALL QUICK ON FROM TO FLAGS
%token	RETURNRST RETURNICMP RETURNICMP6 PROTO INET INET6 ALL ANY ICMPTYPE
%token	ICMP6TYPE CODE KEEP MODULATE STATE PORT RDR NAT BINAT ARROW NODF
%token	MINTTL ERROR ALLOWOPTS FASTROUTE FILENAME ROUTETO DUPTO REPLYTO NO LABEL
%token	NOROUTE FRAGMENT USER GROUP MAXMSS MAXIMUM TTL TOS DROP TABLE
%token	FRAGNORM FRAGDROP FRAGCROP ANCHOR NATANCHOR RDRANCHOR BINATANCHOR
%token	SET OPTIMIZATION TIMEOUT LIMIT LOGINTERFACE BLOCKPOLICY RANDOMID
%token	REQUIREORDER
%token	ANTISPOOF FOR
%token	BITMASK RANDOM SOURCEHASH ROUNDROBIN STATICPORT
%token	ALTQ CBQ PRIQ BANDWIDTH TBRSIZE
%token	QUEUE PRIORITY QLIMIT
%token	<v.string>		STRING
%token	<v.i>			PORTUNARY PORTBINARY
%type	<v.interface>		interface if_list if_item_not if_item
%type	<v.number>		number icmptype icmp6type uid gid
%type	<v.number>		tos not yesno
%type	<v.i>			no dir log af fragcache
%type	<v.i>			staticport
%type	<v.b>			action nataction flags flag blockspec
%type	<v.range>		port rport
%type	<v.hashkey>		hashkey
%type	<v.pooltype>		pooltype
%type	<v.proto>		proto proto_list proto_item
%type	<v.icmp>		icmpspec
%type	<v.icmp>		icmp_list icmp_item
%type	<v.icmp>		icmp6_list icmp6_item
%type	<v.fromto>		fromto
%type	<v.peer>		ipportspec from to
%type	<v.host>		ipspec xhost host dynaddr host_list
%type	<v.host>		redir_host_list redirspec
%type	<v.host>		route_host route_host_list routespec
%type	<v.port>		portspec port_list port_item
%type	<v.uid>			uids uid_list uid_item
%type	<v.gid>			gids gid_list gid_item
%type	<v.route>		route
%type	<v.redirection>		redirection redirpool
%type	<v.string>		label string
%type	<v.keep_state>		keep
%type	<v.state_opt>		state_opt_spec state_opt_list state_opt_item
%type	<v.logquick>		logquick
%type	<v.interface>		antispoof_ifspc antispoof_iflst
%type	<v.qassign>		qname
%type	<v.queue>		qassign qassign_list qassign_item
%type	<v.queue_options>	scheduler
%type	<v.number>		cbqflags_list cbqflags_item
%type	<v.number>		priqflags_list priqflags_item
%type	<v.queue_bwspec>	bandwidth
%type	<v.filter_opts>		filter_opts filter_opt filter_opts_l
%type	<v.queue_opts>		queue_opts queue_opt queue_opts_l
%type	<v.scrub_opts>		scrub_opts scrub_opt scrub_opts_l
%type	<v.table_opts>		table_opts table_opt table_opts_l
%%

ruleset		: /* empty */
		| ruleset '\n'
		| ruleset option '\n'
		| ruleset scrubrule '\n'
		| ruleset natrule '\n'
		| ruleset binatrule '\n'
		| ruleset pfrule '\n'
		| ruleset anchorrule '\n'
		| ruleset altqif '\n'
		| ruleset queuespec '\n'
		| ruleset varset '\n'
		| ruleset antispoof '\n'
		| ruleset tabledef '\n'
		| ruleset error '\n'		{ errors++; }
		;

option		: SET OPTIMIZATION STRING		{
			if (check_rulestate(PFCTL_STATE_OPTION))
				YYERROR;
			if (pfctl_set_optimization(pf, $3) != 0) {
				yyerror("unknown optimization %s", $3);
				YYERROR;
			}
		}
		| SET TIMEOUT timeout_spec
		| SET TIMEOUT '{' timeout_list '}'
		| SET LIMIT limit_spec
		| SET LIMIT '{' limit_list '}'
		| SET LOGINTERFACE STRING		{
			if (check_rulestate(PFCTL_STATE_OPTION))
				YYERROR;
			if ((ifa_exists($3) == NULL) && strcmp($3, "none")) {
				yyerror("interface %s doesn't exist", $3);
				YYERROR;
			}
			if (pfctl_set_logif(pf, $3) != 0) {
				yyerror("error setting loginterface %s", $3);
				YYERROR;
			}
		}
		| SET BLOCKPOLICY DROP	{
			if (pf->opts & PF_OPT_VERBOSE)
				printf("set block-policy drop\n");
			if (check_rulestate(PFCTL_STATE_OPTION))
				YYERROR;
			blockpolicy = PFRULE_DROP;
		}
		| SET BLOCKPOLICY RETURN {
			if (pf->opts & PF_OPT_VERBOSE)
				printf("set block-policy return\n");
			if (check_rulestate(PFCTL_STATE_OPTION))
				YYERROR;
			blockpolicy = PFRULE_RETURN;
		}
		| SET REQUIREORDER yesno {
			if (pf->opts & PF_OPT_VERBOSE)
				printf("set require-order %s\n",
				    $3 == 1 ? "yes" : "no");
			require_order = $3;
		}
		;

string		: string STRING				{
			if (asprintf(&$$, "%s %s", $1, $2) == -1)
				err(1, "string: asprintf");
			free($1);
			free($2);
		}
		| STRING
		;

varset		: STRING PORTUNARY string		{
			if ($2 != PF_OP_EQ)
				YYERROR;
			if (pf->opts & PF_OPT_VERBOSE)
				printf("%s = \"%s\"\n", $1, $3);
			if (symset($1, $3, 0) == -1)
				err(1, "cannot store variable %s", $1);
		}
		;

anchorrule	: ANCHOR string	dir interface af proto fromto {
			struct pf_rule	r;

			if (check_rulestate(PFCTL_STATE_FILTER))
				YYERROR;

			PREPARE_ANCHOR_RULE(r, $2);
			r.direction = $3;
			r.af = $5;

			decide_address_family($7.src.host, &r.af);
			decide_address_family($7.dst.host, &r.af);

			expand_rule(&r, $4, NULL, $6,
			    $7.src.host, $7.src.port, $7.dst.host, $7.dst.port,
			    0, 0, 0);
		}
		| NATANCHOR string interface af proto fromto {
			struct pf_rule	r;

			if (check_rulestate(PFCTL_STATE_NAT))
				YYERROR;

			PREPARE_ANCHOR_RULE(r, $2);
			r.action = PF_NAT;
			r.af = $4;

			decide_address_family($6.src.host, &r.af);
			decide_address_family($6.dst.host, &r.af);

			expand_rule(&r, $3, NULL, $5,
			    $6.src.host, $6.src.port, $6.dst.host, $6.dst.port,
			    0, 0, 0);
		}
		| RDRANCHOR string interface af proto fromto {
			struct pf_rule	r;

			if (check_rulestate(PFCTL_STATE_NAT))
				YYERROR;

			PREPARE_ANCHOR_RULE(r, $2);
			r.action = PF_RDR;
			r.af = $4;

			decide_address_family($6.src.host, &r.af);
			decide_address_family($6.dst.host, &r.af);

			if ($6.src.port != NULL) {
				yyerror("source port parameter not supported"
				    " in rdr-anchor");
				YYERROR;
			}
			if ($6.dst.port != NULL) {
				if ($6.dst.port->next != NULL) {
					yyerror("destination port list "
					    "expansion not supported in "
					    "rdr-anchor");
					YYERROR;
				} else if ($6.dst.port->op != PF_OP_EQ) {
					yyerror("destination port operators"
					    " not supported in rdr-anchor");
					YYERROR;
				}
				r.dst.port[0] = $6.dst.port->port[0];
				r.dst.port[1] = $6.dst.port->port[1];
				r.dst.port_op = $6.dst.port->op;
			}

			expand_rule(&r, $3, NULL, $5,
			    $6.src.host, $6.src.port, $6.dst.host, $6.dst.port,
			    0, 0, 0);
		}
		| BINATANCHOR string interface af proto fromto {
			struct pf_rule	r;

			if (check_rulestate(PFCTL_STATE_NAT))
				YYERROR;

			PREPARE_ANCHOR_RULE(r, $2);
			r.action = PF_BINAT;
			r.af = $4;
			if ($5 != NULL) {
				if ($5->next != NULL) {
					yyerror("proto list expansion"
					    " not supported in binat-anchor");
					YYERROR;
				}
				r.proto = $5->proto;
				free($5);
			}

			if ($6.src.host != NULL || $6.src.port != NULL ||
			    $6.dst.host != NULL || $6.dst.port != NULL) {
				yyerror("fromto parameter not supported"
				    " in binat-anchor");
				YYERROR;
			}

			decide_address_family($6.src.host, &r.af);
			decide_address_family($6.dst.host, &r.af);

			pfctl_add_rule(pf, &r);
		}
		;

scrubrule	: SCRUB dir logquick interface af fromto scrub_opts
		{
			struct pf_rule	r;

			if (check_rulestate(PFCTL_STATE_SCRUB))
				YYERROR;

			memset(&r, 0, sizeof(r));

			r.action = PF_SCRUB;
			r.direction = $2;

			r.log = $3.log;
			if ($3.quick) {
				yyerror("scrub rules do not support 'quick'");
				YYERROR;
			}

			if ($4) {
				if ($4->not) {
					yyerror("scrub rules do not support "
					    "'! <if>'");
					YYERROR;
				}
			}
			r.af = $5;
			if ($7.nodf)
				r.rule_flag |= PFRULE_NODF;
			if ($7.randomid)
				r.rule_flag |= PFRULE_RANDOMID;
			if ($7.minttl)
				r.min_ttl = $7.minttl;
			if ($7.maxmss)
				r.max_mss = $7.maxmss;
			if ($7.fragcache)
				r.rule_flag |= $7.fragcache;

			expand_rule(&r, $4, NULL, NULL,
			    $6.src.host, $6.src.port, $6.dst.host, $6.dst.port,
			    NULL, NULL, NULL);
		}
		;

scrub_opts	:	{
			bzero(&scrub_opts, sizeof scrub_opts);
		}
		  scrub_opts_l
			{ $$ = scrub_opts; }
		| /* empty */ {
			bzero(&scrub_opts, sizeof scrub_opts);
			$$ = scrub_opts;
		}
		;

scrub_opts_l	: scrub_opts_l scrub_opt
		| scrub_opt
		;

scrub_opt	: NODF	{
			if (scrub_opts.nodf) {
				yyerror("no-df cannot be respecified");
				YYERROR;
			}
			scrub_opts.nodf = 1;
		}
		| MINTTL number {
			if (scrub_opts.marker & SOM_MINTTL) {
				yyerror("min-ttl cannot be respecified");
				YYERROR;
			}
			if ($2 > 255) {
				yyerror("illegal min-ttl value %d", $2);
				YYERROR;
			}
			scrub_opts.marker |= SOM_MINTTL;
			scrub_opts.minttl = $2;
		}
		| MAXMSS number {
			if (scrub_opts.marker & SOM_MAXMSS) {
				yyerror("max-mss cannot be respecified");
				YYERROR;
			}
			scrub_opts.marker |= SOM_MAXMSS;
			scrub_opts.maxmss = $2;
		}
		| fragcache {
			if (scrub_opts.marker & SOM_FRAGCACHE) {
				yyerror("fragcache cannot be respecified");
				YYERROR;
			}
			scrub_opts.marker |= SOM_FRAGCACHE;
			scrub_opts.fragcache = $1;
		}
		| RANDOMID {
			if (scrub_opts.randomid) {
				yyerror("random-id cannot be respecified");
				YYERROR;
			}
			scrub_opts.randomid = 1;
		}
		;

fragcache	: FRAGMENT FRAGNORM	{ $$ = 0; /* default */ }
		| FRAGMENT FRAGCROP	{ $$ = PFRULE_FRAGCROP; }
		| FRAGMENT FRAGDROP	{ $$ = PFRULE_FRAGDROP; }
		;

antispoof	: ANTISPOOF logquick antispoof_ifspc af {
			struct pf_rule		 r;
			struct node_host	*h = NULL;
			struct node_if		*i, *j;

			if (check_rulestate(PFCTL_STATE_FILTER))
				YYERROR;

			for (i = $3; i; i = i->next) {
				memset(&r, 0, sizeof(r));

				r.action = PF_DROP;
				r.direction = PF_IN;
				r.log = $2.log;
				r.quick = $2.quick;
				r.af = $4;

				j = calloc(1, sizeof(struct node_if));
				if (j == NULL)
					err(1, "antispoof: calloc");
				if (strlcpy(j->ifname, i->ifname,
				    sizeof(j->ifname)) >= sizeof(j->ifname)) {
					free(j);
					yyerror("interface name too long");
					YYERROR;
				}
				j->not = 1;
				h = ifa_lookup(j->ifname, PFCTL_IFLOOKUP_NET);

				expand_rule(&r, j, NULL, NULL, h, NULL, NULL,
				    NULL, NULL, NULL, NULL);

				if ((i->ifa_flags & IFF_LOOPBACK) == 0) {
					memset(&r, 0, sizeof(r));

					r.action = PF_DROP;
					r.direction = PF_IN;
					r.log = $2.log;
					r.quick = $2.quick;
					r.af = $4;

					h = ifa_lookup(i->ifname,
					    PFCTL_IFLOOKUP_HOST);

					expand_rule(&r, NULL, NULL, NULL, h,
					    NULL, NULL, NULL, NULL, NULL, NULL);
				}
			}
		}
		;

antispoof_ifspc	: FOR if_item			{ $$ = $2; }
		| FOR '{' antispoof_iflst '}'	{ $$ = $3; }
		;

antispoof_iflst	: if_item			{ $$ = $1; }
		| antispoof_iflst comma if_item	{
			$1->tail->next = $3;
			$1->tail = $3;
			$$ = $1;
		}
		;

not		: '!'		{ $$ = 1; }
		| /* empty */	{ $$ = 0; }

tabledef	: TABLE PORTUNARY STRING PORTUNARY table_opts {
			if ($2 != PF_OP_LT || $4 != PF_OP_GT)
				YYERROR;
			if (strlen($3) >= PF_TABLE_NAME_SIZE) {
				yyerror("table name too long, max %d chars",
				    PF_TABLE_NAME_SIZE - 1);
				YYERROR;
			}
			pfctl_define_table($3, $5.flags, $5.init_addr,
			    (pf->opts & PF_OPT_NOACTION) || !(pf->loadopt &
				(PFCTL_FLAG_TABLE | PFCTL_FLAG_ALL)));
		}
		;

table_opts	:	{
			bzero(&table_opts, sizeof table_opts);
		}
		   table_opts_l
			{ $$ = table_opts; }
		| /* empty */
			{
			bzero(&table_opts, sizeof table_opts);
			$$ = table_opts;
		}
		;

table_opts_l	: table_opts_l table_opt
		| table_opt
		;

table_opt	: STRING
			{
			if (!strcmp($1, "const"))
				table_opts.flags |= PFR_TFLAG_CONST;
			else if (!strcmp($1, "persist"))
				table_opts.flags |= PFR_TFLAG_PERSIST;
			else
				YYERROR;
		}
		| '{' tableaddrs '}'	{ table_opts.init_addr = 1; }
		| FILENAME STRING	{
			pfctl_append_file($2);
			table_opts.init_addr = 1;
		}
		;

tableaddrs	: /* empty */
		| tableaddrs tableaddr comma

tableaddr	: not STRING {
			pfctl_append_addr($2, -1, $1);
		}
		| not STRING '/' number {
			pfctl_append_addr($2, $4, $1);
		}
		;

altqif		: ALTQ interface queue_opts QUEUE qassign {
			struct pf_altq	a;

			if (check_rulestate(PFCTL_STATE_QUEUE))
				YYERROR;

			memset(&a, 0, sizeof(a));
			if ($3.scheduler.qtype == ALTQT_NONE) {
				yyerror("no scheduler specified!");
				YYERROR;
			}
			a.scheduler = $3.scheduler.qtype;
			switch (a.scheduler) {
			case ALTQT_CBQ:
				a.pq_u.cbq_opts =
				    $3.scheduler.data.cbq_opts;
				break;
			case ALTQT_PRIQ:
				a.pq_u.priq_opts =
				    $3.scheduler.data.priq_opts;
				break;
			default:
				break;
			}
			a.qlimit = $3.qlimit;
			a.tbrsize = $3.tbrsize;
			if ($5 == NULL) {
				yyerror("no child queues specified");
				YYERROR;
			}
			if (expand_altq(&a, $2, $5, $3.queue_bwspec))
				YYERROR;
		}
		;

queuespec	: QUEUE STRING queue_opts qassign {
			struct pf_altq	a;

			if (check_rulestate(PFCTL_STATE_QUEUE))
				YYERROR;

			memset(&a, 0, sizeof(a));

			if (strlcpy(a.qname, $2, sizeof(a.qname)) >=
			    sizeof(a.qname)) {
				yyerror("queue name too long (max "
				    "%d chars)", PF_QNAME_SIZE-1);
				YYERROR;
			}
			if ($3.tbrsize) {
				yyerror("cannot specify tbrsize for queue");
				YYERROR;
			}
			if ($3.priority > 255) {
				yyerror("priority out of range: max 255");
				YYERROR;
			}
			a.priority = $3.priority;
			a.qlimit = $3.qlimit;
			a.scheduler = $3.scheduler.qtype;
			switch (a.scheduler) {
			case ALTQT_CBQ:
				a.pq_u.cbq_opts =
				    $3.scheduler.data.cbq_opts;
				break;
			case ALTQT_PRIQ:
				a.pq_u.priq_opts =
				    $3.scheduler.data.priq_opts;
				break;
			default:
				break;
			}
			if (expand_queue(&a, $4, $3.queue_bwspec))
				YYERROR;
		}
		;

queue_opts	:	{
			bzero(&queue_opts, sizeof queue_opts);
			queue_opts.priority = DEFAULT_PRIORITY;
			queue_opts.qlimit = DEFAULT_QLIMIT;
			queue_opts.scheduler.qtype = ALTQT_NONE;
			queue_opts.queue_bwspec.bw_percent = 100;
		}
		  queue_opts_l
			{ $$ = queue_opts; }
		| /* empty */ {
			bzero(&queue_opts, sizeof queue_opts);
			queue_opts.priority = DEFAULT_PRIORITY;
			queue_opts.qlimit = DEFAULT_QLIMIT;
			queue_opts.scheduler.qtype = ALTQT_NONE;
			queue_opts.queue_bwspec.bw_percent = 100;
			$$ = queue_opts;
		}
		;

queue_opts_l	: queue_opts_l queue_opt
		| queue_opt
		;

queue_opt	: bandwidth	{
			if (queue_opts.marker & QOM_BWSPEC) {
				yyerror("bandwidth cannot be respecified");
				YYERROR;
			}
			queue_opts.marker |= QOM_BWSPEC;
			queue_opts.queue_bwspec = $1;
		}
		| PRIORITY number	{
			if (queue_opts.marker & QOM_PRIORITY) {
				yyerror("priority cannot be respecified");
				YYERROR;
			}
			if ($2 > 255) {
				yyerror("priority out of range: max 255");
				YYERROR;
			}
			queue_opts.marker |= QOM_PRIORITY;
			queue_opts.priority = $2;
		}
		| QLIMIT number	{
			if (queue_opts.marker & QOM_QLIMIT) {
				yyerror("qlimit cannot be respecified");
				YYERROR;
			}
			if ($2 > 65535) {
				yyerror("qlimit out of range: max 65535");
				YYERROR;
			}
			queue_opts.marker |= QOM_QLIMIT;
			queue_opts.qlimit = $2;
		}
		| scheduler	{
			if (queue_opts.marker & QOM_SCHEDULER) {
				yyerror("scheduler cannot be respecified");
				YYERROR;
			}
			queue_opts.marker |= QOM_SCHEDULER;
			queue_opts.scheduler = $1;
		}
		| TBRSIZE number	{
			if (queue_opts.marker & QOM_TBRSIZE) {
				yyerror("tbrsize cannot be respecified");
				YYERROR;
			}
			if ($2 > 65535) {
				yyerror("tbrsize too big: max 65535");
				YYERROR;
			}
			queue_opts.marker |= QOM_TBRSIZE;
			queue_opts.tbrsize = $2;
		}
		;

bandwidth	: BANDWIDTH STRING {
			double	 bps;
			char	*cp;

			$$.bw_percent = 0;

			bps = strtod($2, &cp);
			if (cp != NULL) {
				if (!strcmp(cp, "b"))
					; /* nothing */
				else if (!strcmp(cp, "Kb"))
					bps *= 1000;
				else if (!strcmp(cp, "Mb"))
					bps *= 1000 * 1000;
				else if (!strcmp(cp, "Gb"))
					bps *= 1000 * 1000 * 1000;
				else if (!strcmp(cp, "%")) {
					if (bps < 0 || bps > 100) {
						yyerror("bandwidth spec "
						    "out of range");
						YYERROR;
					}
					$$.bw_percent = bps;
					bps = 0;
				} else {
					yyerror("unknown unit %s", cp);
					YYERROR;
				}
			}
			$$.bw_absolute = (u_int32_t)bps;
		}

scheduler	: CBQ				{
			$$.qtype = ALTQT_CBQ;
			$$.data.cbq_opts.flags = 0;
		}
		| CBQ '(' cbqflags_list ')'	{
			$$.qtype = ALTQT_CBQ;
			$$.data.cbq_opts.flags = $3;
		}
		| PRIQ				{
			$$.qtype = ALTQT_PRIQ;
			$$.data.priq_opts.flags = 0;
		}
		| PRIQ '(' priqflags_list ')'	{
			$$.qtype = ALTQT_PRIQ;
			$$.data.priq_opts.flags = $3;
		}
		;

cbqflags_list	: cbqflags_item				{ $$ |= $1; }
		| cbqflags_list comma cbqflags_item	{ $$ |= $3; }
		;

cbqflags_item	: STRING	{
			if (!strcmp($1, "default"))
				$$ = CBQCLF_DEFCLASS;
			else if (!strcmp($1, "borrow"))
				$$ = CBQCLF_BORROW;
			else if (!strcmp($1, "red"))
				$$ = CBQCLF_RED;
			else if (!strcmp($1, "ecn"))
				$$ = CBQCLF_RED|CBQCLF_ECN;
			else if (!strcmp($1, "rio"))
				$$ = CBQCLF_RIO;
			else {
				yyerror("unknown cbq flag \"%s\"", $1);
				YYERROR;
			}
		}
		;

priqflags_list	: priqflags_item			{ $$ |= $1; }
		| priqflags_list comma priqflags_item	{ $$ |= $3; }
		;

priqflags_item	: STRING	{
			if (!strcmp($1, "default"))
				$$ = PRCF_DEFAULTCLASS;
			else if (!strcmp($1, "red"))
				$$ = PRCF_RED;
			else if (!strcmp($1, "ecn"))
				$$ = PRCF_RED|PRCF_ECN;
			else if (!strcmp($1, "rio"))
				$$ = PRCF_RIO;
			else {
				yyerror("unknown priq flag \"%s\"", $1);
				YYERROR;
			}
		}
		;

qassign		: /* empty */		{ $$ = NULL; }
		| qassign_item		{ $$ = $1; }
		| '{' qassign_list '}'	{ $$ = $2; }
		;

qassign_list	: qassign_item			{ $$ = $1; }
		| qassign_list comma qassign_item	{
			$1->tail->next = $3;
			$1->tail = $3;
			$$ = $1;
		}
		;

qassign_item	: STRING			{
			$$ = calloc(1, sizeof(struct node_queue));
			if ($$ == NULL)
				err(1, "qassign_item: calloc");
			if (strlcpy($$->queue, $1, sizeof($$->queue)) >=
			    sizeof($$->queue)) {
				free($$);
				yyerror("queue name '%s' too long (max "
				    "%d chars)", $1, sizeof($$->queue)-1);
				YYERROR;
			}
			$$->next = NULL;
			$$->tail = $$;
		}
		;

pfrule		: action dir logquick interface route af proto fromto
		  filter_opts
		{
			struct pf_rule		 r;
			struct node_state_opt	*o;
			struct node_proto	*proto;

			if (check_rulestate(PFCTL_STATE_FILTER))
				YYERROR;

			memset(&r, 0, sizeof(r));

			r.action = $1.b1;
			switch ($1.b2) {
			case PFRULE_RETURNRST:
				r.rule_flag |= PFRULE_RETURNRST;
				r.return_ttl = $1.w;
				break;
			case PFRULE_RETURNICMP:
				r.rule_flag |= PFRULE_RETURNICMP;
				r.return_icmp = $1.w;
				r.return_icmp6 = $1.w2;
				break;
			case PFRULE_RETURN:
				r.rule_flag |= PFRULE_RETURN;
				r.return_icmp = $1.w;
				r.return_icmp6 = $1.w2;
				break;
			}
			r.direction = $2;
			r.log = $3.log;
			r.quick = $3.quick;

			r.af = $6;
			r.flags = $9.flags.b1;
			r.flagset = $9.flags.b2;

			if ($9.flags.b1 || $9.flags.b2) {
				for (proto = $7; proto != NULL &&
				    proto->proto != IPPROTO_TCP;
				    proto = proto->next)
					;	/* nothing */
				if (proto == NULL && $7 != NULL) {
					yyerror("flags only apply to tcp");
					YYERROR;
				}
			}

			r.tos = $9.tos;
			r.keep_state = $9.keep.action;
			o = $9.keep.options;
			while (o) {
				struct node_state_opt	*p = o;

				switch (o->type) {
				case PF_STATE_OPT_MAX:
					if (r.max_states) {
						yyerror("state option 'max' "
						    "multiple definitions");
						YYERROR;
					}
					r.max_states = o->data.max_states;
					break;
				case PF_STATE_OPT_TIMEOUT:
					if (r.timeout[o->data.timeout.number]) {
						yyerror("state timeout %s "
						    "multiple definitions",
						    pf_timeouts[o->data.
						    timeout.number].name);
						YYERROR;
					}
					r.timeout[o->data.timeout.number] =
					    o->data.timeout.seconds;
				}
				o = o->next;
				free(p);
			}

			if ($9.fragment)
				r.rule_flag |= PFRULE_FRAGMENT;
			r.allow_opts = $9.allowopts;

			decide_address_family($8.src.host, &r.af);
			decide_address_family($8.dst.host, &r.af);

			if ($5.rt) {
				if (!r.direction) {
					yyerror("direction must be explicit "
					    "with rules that specify routing");
					YYERROR;
				}
				r.rt = $5.rt;
				r.rpool.opts = $5.pool_opts;
				if ($5.key != NULL)
					memcpy(&r.rpool.key, $5.key,
					    sizeof(struct pf_poolhashkey));
			}

			if (r.rt && r.rt != PF_FASTROUTE) {
				decide_address_family($5.host, &r.af);
				remove_invalid_hosts(&$5.host, &r.af);
				if ($5.host == NULL) {
					yyerror("no routing address with "
					    "matching address family found.");
					YYERROR;
				}
				if ($5.host->next != NULL) {
					if (r.rpool.opts == PF_POOL_NONE)
						r.rpool.opts =
						    PF_POOL_ROUNDROBIN;
					if (r.rpool.opts !=
					    PF_POOL_ROUNDROBIN) {
						yyerror("r.rpool.opts must "
						    "be PF_POOL_ROUNDROBIN");
						YYERROR;
					}
				}
			}

			if ($9.label) {
				if (strlcpy(r.label, $9.label,
				    sizeof(r.label)) >= sizeof(r.label)) {
					yyerror("rule label too long (max "
					    "%d chars)", sizeof(r.label)-1);
					YYERROR;
				}
				free($9.label);
			}

			if ($9.queues.qname != NULL) {
				if (strlcpy(r.qname, $9.queues.qname,
				    sizeof(r.qname)) >= sizeof(r.qname)) {
					yyerror("rule qname too long (max "
					    "%d chars)", sizeof(r.qname)-1);
					YYERROR;
				}
				free($9.queues.qname);
			}
			if ($9.queues.pqname != NULL) {
				if (strlcpy(r.pqname, $9.queues.pqname,
				    sizeof(r.pqname)) >= sizeof(r.pqname)) {
					yyerror("rule pqname too long (max "
					    "%d chars)", sizeof(r.pqname)-1);
					YYERROR;
				}
				free($9.queues.pqname);
			}

			expand_rule(&r, $4, $5.host, $7,
			    $8.src.host, $8.src.port, $8.dst.host, $8.dst.port,
			    $9.uid, $9.gid, $9.icmpspec);
		}
		;

filter_opts	:	{ bzero(&filter_opts, sizeof filter_opts); }
		  filter_opts_l
			{ $$ = filter_opts; }
		| /* empty */	{
			bzero(&filter_opts, sizeof filter_opts);
			$$ = filter_opts;
		}
		;

filter_opts_l	: filter_opts_l filter_opt
		| filter_opt
		;

filter_opt	: USER uids {
			if (filter_opts.uid)
				$2->tail->next = filter_opts.uid;
			filter_opts.uid = $2;
		}
		| GROUP gids {
			if (filter_opts.gid)
				$2->tail->next = filter_opts.gid;
			filter_opts.gid = $2;
		}
		| flags {
			if (filter_opts.marker & FOM_FLAGS) {
				yyerror("flags cannot be redefined");
				YYERROR;
			}
			filter_opts.marker |= FOM_FLAGS;
			filter_opts.flags.b1 |= $1.b1;
			filter_opts.flags.b2 |= $1.b2;
			filter_opts.flags.w |= $1.w;
			filter_opts.flags.w2 |= $1.w2;
		}
		| icmpspec {
			if (filter_opts.marker & FOM_ICMP) {
				yyerror("icmp-type cannot be redefined");
				YYERROR;
			}
			filter_opts.marker |= FOM_ICMP;
			filter_opts.icmpspec = $1;
		}
		| tos {
			if (filter_opts.marker & FOM_TOS) {
				yyerror("tos cannot be redefined");
				YYERROR;
			}
			filter_opts.marker |= FOM_TOS;
			filter_opts.tos = $1;
		}
		| keep {
			if (filter_opts.marker & FOM_KEEP) {
				yyerror("modulate or keep cannot be redefined");
				YYERROR;
			}
			filter_opts.marker |= FOM_KEEP;
			filter_opts.keep.action = $1.action;
			filter_opts.keep.options = $1.options;
		}
		| FRAGMENT {
			filter_opts.fragment = 1;
		}
		| ALLOWOPTS {
			filter_opts.allowopts = 1;
		}
		| label	{
			if (filter_opts.label) {
				yyerror("label cannot be redefined");
				YYERROR;
			}
			filter_opts.label = $1;
		}
		| qname	{
			if (filter_opts.queues.qname) {
				yyerror("queue cannot be redefined");
				YYERROR;
			}
			filter_opts.queues = $1;
		}
		;

action		: PASS			{ $$.b1 = PF_PASS; $$.b2 = $$.w = 0; }
		| BLOCK blockspec	{ $$ = $2; $$.b1 = PF_DROP; }
		;

blockspec	: /* empty */		{
			$$.b2 = blockpolicy;
			$$.w = returnicmpdefault;
			$$.w2 = returnicmp6default;
		}
		| DROP			{
			$$.b2 = PFRULE_DROP;
			$$.w = 0;
			$$.w2 = 0;
		}
		| RETURNRST		{
			$$.b2 = PFRULE_RETURNRST;
			$$.w = 0;
			$$.w2 = 0;
		}
		| RETURNRST '(' TTL number ')'	{
			$$.b2 = PFRULE_RETURNRST;
			$$.w = $4;
			$$.w2 = 0;
		}
		| RETURNICMP		{
			$$.b2 = PFRULE_RETURNICMP;
			$$.w = returnicmpdefault;
			$$.w2 = returnicmp6default;
		}
		| RETURNICMP6		{
			$$.b2 = PFRULE_RETURNICMP;
			$$.w = returnicmpdefault;
			$$.w2 = returnicmp6default;
		}
		| RETURNICMP '(' STRING ')'	{
			$$.b2 = PFRULE_RETURNICMP;
			if (!($$.w = parseicmpspec($3, AF_INET)))
				YYERROR;
			$$.w2 = returnicmp6default;
		}
		| RETURNICMP6 '(' STRING ')'	{
			$$.b2 = PFRULE_RETURNICMP;
			$$.w = returnicmpdefault;
			if (!($$.w2 = parseicmpspec($3, AF_INET6)))
				YYERROR;
		}
		| RETURNICMP '(' STRING comma STRING ')' {
			$$.b2 = PFRULE_RETURNICMP;
			if (!($$.w = parseicmpspec($3, AF_INET)))
				YYERROR;
			if (!($$.w2 = parseicmpspec($5, AF_INET6)))
				YYERROR;
		}
		| RETURN {
			$$.b2 = PFRULE_RETURN;
			$$.w = returnicmpdefault;
			$$.w2 = returnicmp6default;
		}
		;

dir		: /* empty */			{ $$ = 0; }
		| IN				{ $$ = PF_IN; }
		| OUT				{ $$ = PF_OUT; }
		;

logquick	: /* empty */			{ $$.log = 0; $$.quick = 0; }
		| log				{ $$.log = $1; $$.quick = 0; }
		| QUICK				{ $$.log = 0; $$.quick = 1; }
		| log QUICK			{ $$.log = $1; $$.quick = 1; }
		| QUICK log			{ $$.log = $2; $$.quick = 1; }
		;

log		: LOG				{ $$ = 1; }
		| LOGALL			{ $$ = 2; }
		;

interface	: /* empty */			{ $$ = NULL; }
		| ON if_item_not		{ $$ = $2; }
		| ON '{' if_list '}'		{ $$ = $3; }
		;

if_list		: if_item_not			{ $$ = $1; }
		| if_list comma if_item_not	{
			$1->tail->next = $3;
			$1->tail = $3;
			$$ = $1;
		}
		;

if_item_not	: not if_item			{ $$ = $2; $$->not = $1; }
		;

if_item		: STRING			{
			struct node_host	*n;

			if ((n = ifa_exists($1)) == NULL) {
				yyerror("unknown interface %s", $1);
				YYERROR;
			}
			$$ = calloc(1, sizeof(struct node_if));
			if ($$ == NULL)
				err(1, "if_item: calloc");
			if (strlcpy($$->ifname, $1, sizeof($$->ifname)) >=
			    sizeof($$->ifname)) {
				free($$);
				yyerror("interface name too long");
				YYERROR;
			}
			$$->ifa_flags = n->ifa_flags;
			$$->not = 0;
			$$->next = NULL;
			$$->tail = $$;
		}
		;

af		: /* empty */			{ $$ = 0; }
		| INET				{ $$ = AF_INET; }
		| INET6				{ $$ = AF_INET6; }

proto		: /* empty */			{ $$ = NULL; }
		| PROTO proto_item		{ $$ = $2; }
		| PROTO '{' proto_list '}'	{ $$ = $3; }
		;

proto_list	: proto_item			{ $$ = $1; }
		| proto_list comma proto_item	{
			$1->tail->next = $3;
			$1->tail = $3;
			$$ = $1;
		}
		;

proto_item	: STRING			{
			u_int8_t	pr;
			u_long		ulval;

			if (atoul($1, &ulval) == 0) {
				if (ulval > 255) {
					yyerror("protocol outside range");
					YYERROR;
				}
				pr = (u_int8_t)ulval;
			} else {
				struct protoent	*p;

				p = getprotobyname($1);
				if (p == NULL) {
					yyerror("unknown protocol %s", $1);
					YYERROR;
				}
				pr = p->p_proto;
			}
			if (pr == 0) {
				yyerror("proto 0 cannot be used");
				YYERROR;
			}
			$$ = calloc(1, sizeof(struct node_proto));
			if ($$ == NULL)
				err(1, "proto_item: calloc");
			$$->proto = pr;
			$$->next = NULL;
			$$->tail = $$;
		}
		;

fromto		: ALL				{
			$$.src.host = NULL;
			$$.src.port = NULL;
			$$.dst.host = NULL;
			$$.dst.port = NULL;
		}
		| from to			{
			$$.src = $1;
			$$.dst = $2;
		}
		;

from		: /* empty */			{
			$$.host = NULL;
			$$.port = NULL;
		}
		| FROM ipportspec		{
			$$ = $2;
		}
		;

to		: /* empty */			{
			$$.host = NULL;
			$$.port = NULL;
		}
		| TO ipportspec		{
			$$ = $2;
		}
		;

ipportspec	: ipspec			{
			$$.host = $1;
			$$.port = NULL;
		}
		| ipspec PORT portspec		{
			$$.host = $1;
			$$.port = $3;
		}
		| PORT portspec			{
			$$.host = NULL;
			$$.port = $2;
		}
		;

ipspec		: ANY				{ $$ = NULL; }
		| xhost				{ $$ = $1; }
		| '{' host_list '}'		{ $$ = $2; }
		;

host_list	: xhost				{ $$ = $1; }
		| host_list comma xhost		{
			/* $3 may be a list, so use its tail pointer */
			if ($3 == NULL)
				$$ = $1;
			else if ($1 == NULL)
				$$ = $3;
			else {
				$1->tail->next = $3->tail;
				$1->tail = $3->tail;
				$$ = $1;
			}
		}
		;

xhost		: not host			{
			struct node_host	*n;

			for (n = $2; n != NULL; n = n->next)
				n->not = $1;
			$$ = $2;
		}
		| NOROUTE			{
			$$ = calloc(1, sizeof(struct node_host));
			if ($$ == NULL)
				err(1, "xhost: calloc");
			$$->addr.type = PF_ADDR_NOROUTE;
			$$->next = NULL;
			$$->tail = $$;
		}
		;

host		: STRING			{ $$ = host($1, -1); }
		| STRING '/' number		{ $$ = host($1, $3); }
		| dynaddr
		| dynaddr '/' number		{
			struct node_host	*n;

			$$ = $1;
			for (n = $1; n != NULL; n = n->next)
				set_ipmask(n, $3);
		}
		| PORTUNARY STRING PORTUNARY	{
			if ($1 != PF_OP_LT || $3 != PF_OP_GT)
				YYERROR;
			if (strlen($2) >= PF_TABLE_NAME_SIZE) {
				yyerror("table name '%s' too long");
				YYERROR;
			}
			$$ = calloc(1, sizeof(struct node_host));
			if ($$ == NULL)
				err(1, "host: calloc");
			$$->addr.type = PF_ADDR_TABLE;
			if (strlcpy($$->addr.v.tblname, $2,
			    sizeof($$->addr.v.tblname)) >=
			    sizeof($$->addr.v.tblname))
				errx(1, "host: strlcpy");
			$$->next = NULL;
			$$->tail = $$;
		}
		;

number		: STRING			{
			u_long	ulval;

			if (atoul($1, &ulval) == -1) {
				yyerror("%s is not a number", $1);
				YYERROR;
			} else
				$$ = ulval;
		}
		;

dynaddr		: '(' STRING ')'		{
			if (ifa_exists($2) == NULL) {
				yyerror("interface %s does not exist", $2);
				YYERROR;
			}
			$$ = calloc(1, sizeof(struct node_host));
			if ($$ == NULL)
				err(1, "address: calloc");
			$$->af = 0;
			set_ipmask($$, 128);
			$$->addr.type = PF_ADDR_DYNIFTL;
			if (strlcpy($$->addr.v.ifname, $2,
			    sizeof($$->addr.v.ifname)) >=
			    sizeof($$->addr.v.ifname)) {
				free($$);
				yyerror("interface name too long");
				YYERROR;
			}
			$$->next = NULL;
			$$->tail = $$;
		}
		;

portspec	: port_item			{ $$ = $1; }
		| '{' port_list '}'		{ $$ = $2; }
		;

port_list	: port_item			{ $$ = $1; }
		| port_list comma port_item	{
			$1->tail->next = $3;
			$1->tail = $3;
			$$ = $1;
		}
		;

port_item	: port				{
			$$ = calloc(1, sizeof(struct node_port));
			if ($$ == NULL)
				err(1, "port_item: calloc");
			$$->port[0] = $1.a;
			$$->port[1] = $1.b;
			if ($1.t)
				$$->op = PF_OP_RRG;
			else
				$$->op = PF_OP_EQ;
			$$->next = NULL;
			$$->tail = $$;
		}
		| PORTUNARY port		{
			if ($2.t) {
				yyerror("':' cannot be used with an other "
				    "port operator");
				YYERROR;
			}
			$$ = calloc(1, sizeof(struct node_port));
			if ($$ == NULL)
				err(1, "port_item: calloc");
			$$->port[0] = $2.a;
			$$->port[1] = $2.b;
			$$->op = $1;
			$$->next = NULL;
			$$->tail = $$;
		}
		| port PORTBINARY port		{
			if ($1.t || $3.t) {
				yyerror("':' cannot be used with an other "
				    "port operator");
				YYERROR;
			}
			$$ = calloc(1, sizeof(struct node_port));
			if ($$ == NULL)
				err(1, "port_item: calloc");
			$$->port[0] = $1.a;
			$$->port[1] = $3.a;
			$$->op = $2;
			$$->next = NULL;
			$$->tail = $$;
		}
		;

port		: STRING			{
			char	*p = strchr($1, ':');
			struct servent	*s = NULL;
			u_long		 ulval;

			if (p == NULL) {
				if (atoul($1, &ulval) == 0) {
					if (ulval > 65535) {
						yyerror("illegal port value %d",
						    ulval);
						YYERROR;
					}
					$$.a = htons(ulval);
				} else {
					s = getservbyname($1, "tcp");
					if (s == NULL)
						s = getservbyname($1, "udp");
					if (s == NULL) {
						yyerror("unknown port %s", $1);
						YYERROR;
					}
					$$.a = s->s_port;
				}
				$$.b = 0;
				$$.t = 0;
			} else {
				int port[2];

				*p++ = 0;
				if ((port[0] = getservice($1)) == -1 ||
				    (port[1] = getservice(p)) == -1)
					YYERROR;
				$$.a = port[0];
				$$.b = port[1];
				$$.t = PF_OP_RRG;
			}
		}
		;

uids		: uid_item			{ $$ = $1; }
		| '{' uid_list '}'		{ $$ = $2; }
		;

uid_list	: uid_item			{ $$ = $1; }
		| uid_list comma uid_item	{
			$1->tail->next = $3;
			$1->tail = $3;
			$$ = $1;
		}
		;

uid_item	: uid				{
			$$ = calloc(1, sizeof(struct node_uid));
			if ($$ == NULL)
				err(1, "uid_item: calloc");
			$$->uid[0] = $1;
			$$->uid[1] = $1;
			$$->op = PF_OP_EQ;
			$$->next = NULL;
			$$->tail = $$;
		}
		| PORTUNARY uid			{
			if ($2 == UID_MAX && $1 != PF_OP_EQ && $1 != PF_OP_NE) {
				yyerror("user unknown requires operator = or "
				    "!=");
				YYERROR;
			}
			$$ = calloc(1, sizeof(struct node_uid));
			if ($$ == NULL)
				err(1, "uid_item: calloc");
			$$->uid[0] = $2;
			$$->uid[1] = $2;
			$$->op = $1;
			$$->next = NULL;
			$$->tail = $$;
		}
		| uid PORTBINARY uid		{
			if ($1 == UID_MAX || $3 == UID_MAX) {
				yyerror("user unknown requires operator = or "
				    "!=");
				YYERROR;
			}
			$$ = calloc(1, sizeof(struct node_uid));
			if ($$ == NULL)
				err(1, "uid_item: calloc");
			$$->uid[0] = $1;
			$$->uid[1] = $3;
			$$->op = $2;
			$$->next = NULL;
			$$->tail = $$;
		}
		;

uid		: STRING			{
			u_long	ulval;

			if (atoul($1, &ulval) == -1) {
				if (!strcmp($1, "unknown"))
					$$ = UID_MAX;
				else {
					struct passwd	*pw;

					if ((pw = getpwnam($1)) == NULL) {
						yyerror("unknown user %s", $1);
						YYERROR;
					}
					$$ = pw->pw_uid;
				}
			} else {
				if (ulval >= UID_MAX) {
					yyerror("illegal uid value %lu", ulval);
					YYERROR;
				}
				$$ = ulval;
			}
		}
		;

gids		: gid_item			{ $$ = $1; }
		| '{' gid_list '}'		{ $$ = $2; }
		;

gid_list	: gid_item			{ $$ = $1; }
		| gid_list comma gid_item	{
			$1->tail->next = $3;
			$1->tail = $3;
			$$ = $1;
		}
		;

gid_item	: gid				{
			$$ = calloc(1, sizeof(struct node_gid));
			if ($$ == NULL)
				err(1, "gid_item: calloc");
			$$->gid[0] = $1;
			$$->gid[1] = $1;
			$$->op = PF_OP_EQ;
			$$->next = NULL;
			$$->tail = $$;
		}
		| PORTUNARY gid			{
			if ($2 == GID_MAX && $1 != PF_OP_EQ && $1 != PF_OP_NE) {
				yyerror("group unknown requires operator = or "
				    "!=");
				YYERROR;
			}
			$$ = calloc(1, sizeof(struct node_gid));
			if ($$ == NULL)
				err(1, "gid_item: calloc");
			$$->gid[0] = $2;
			$$->gid[1] = $2;
			$$->op = $1;
			$$->next = NULL;
			$$->tail = $$;
		}
		| gid PORTBINARY gid		{
			if ($1 == GID_MAX || $3 == GID_MAX) {
				yyerror("group unknown requires operator = or "
				    "!=");
				YYERROR;
			}
			$$ = calloc(1, sizeof(struct node_gid));
			if ($$ == NULL)
				err(1, "gid_item: calloc");
			$$->gid[0] = $1;
			$$->gid[1] = $3;
			$$->op = $2;
			$$->next = NULL;
			$$->tail = $$;
		}
		;

gid		: STRING			{
			u_long	ulval;

			if (atoul($1, &ulval) == -1) {
				if (!strcmp($1, "unknown"))
					$$ = GID_MAX;
				else {
					struct group	*grp;

					if ((grp = getgrnam($1)) == NULL) {
						yyerror("unknown group %s", $1);
						YYERROR;
					}
					$$ = grp->gr_gid;
				}
			} else {
				if (ulval >= GID_MAX) {
					yyerror("illegal gid value %lu", ulval);
					YYERROR;
				}
				$$ = ulval;
			}
		}
		;

flag		: STRING			{
			int	f;

			if ((f = parse_flags($1)) < 0) {
				yyerror("bad flags %s", $1);
				YYERROR;
			}
			$$.b1 = f;
		}
		;

flags		: FLAGS flag '/' flag	{ $$.b1 = $2.b1; $$.b2 = $4.b1; }
		| FLAGS '/' flag	{ $$.b1 = 0; $$.b2 = $3.b1; }
		;

icmpspec	: ICMPTYPE icmp_item		{ $$ = $2; }
		| ICMPTYPE '{' icmp_list '}'	{ $$ = $3; }
		| ICMP6TYPE icmp6_item		{ $$ = $2; }
		| ICMP6TYPE '{' icmp6_list '}'	{ $$ = $3; }
		;

icmp_list	: icmp_item			{ $$ = $1; }
		| icmp_list comma icmp_item	{
			$1->tail->next = $3;
			$1->tail = $3;
			$$ = $1;
		}
		;

icmp6_list	: icmp6_item			{ $$ = $1; }
		| icmp6_list comma icmp6_item	{
			$1->tail->next = $3;
			$1->tail = $3;
			$$ = $1;
		}
		;

icmp_item	: icmptype		{
			$$ = calloc(1, sizeof(struct node_icmp));
			if ($$ == NULL)
				err(1, "icmp_item: calloc");
			$$->type = $1;
			$$->code = 0;
			$$->proto = IPPROTO_ICMP;
			$$->next = NULL;
			$$->tail = $$;
		}
		| icmptype CODE STRING	{
			const struct icmpcodeent	*p;
			u_long				 ulval;

			if (atoul($3, &ulval) == 0) {
				if (ulval > 255) {
					yyerror("illegal icmp-code %d", ulval);
					YYERROR;
				}
			} else {
				if ((p = geticmpcodebyname($1-1, $3,
				    AF_INET)) == NULL) {
					yyerror("unknown icmp-code %s", $3);
					YYERROR;
				}
				ulval = p->code;
			}
			$$ = calloc(1, sizeof(struct node_icmp));
			if ($$ == NULL)
				err(1, "icmp_item: calloc");
			$$->type = $1;
			$$->code = ulval + 1;
			$$->proto = IPPROTO_ICMP;
			$$->next = NULL;
			$$->tail = $$;
		}
		;

icmp6_item	: icmp6type		{
			$$ = calloc(1, sizeof(struct node_icmp));
			if ($$ == NULL)
				err(1, "icmp_item: calloc");
			$$->type = $1;
			$$->code = 0;
			$$->proto = IPPROTO_ICMPV6;
			$$->next = NULL;
			$$->tail = $$;
		}
		| icmp6type CODE STRING	{
			const struct icmpcodeent	*p;
			u_long				 ulval;

			if (atoul($3, &ulval) == 0) {
				if (ulval > 255) {
					yyerror("illegal icmp6-code %ld",
					    ulval);
					YYERROR;
				}
			} else {
				if ((p = geticmpcodebyname($1-1, $3,
				    AF_INET6)) == NULL) {
					yyerror("unknown icmp6-code %s", $3);
					YYERROR;
				}
				ulval = p->code;
			}
			$$ = calloc(1, sizeof(struct node_icmp));
			if ($$ == NULL)
				err(1, "icmp_item: calloc");
			$$->type = $1;
			$$->code = ulval + 1;
			$$->proto = IPPROTO_ICMPV6;
			$$->next = NULL;
			$$->tail = $$;
		}
		;

icmptype	: STRING			{
			const struct icmptypeent	*p;
			u_long				 ulval;

			if (atoul($1, &ulval) == 0) {
				if (ulval > 255) {
					yyerror("illegal icmp-type %d", ulval);
					YYERROR;
				}
				$$ = ulval + 1;
			} else {
				if ((p = geticmptypebyname($1, AF_INET)) ==
				    NULL) {
					yyerror("unknown icmp-type %s", $1);
					YYERROR;
				}
				$$ = p->type + 1;
			}
		}
		;

icmp6type	: STRING			{
			const struct icmptypeent	*p;
			u_long				 ulval;

			if (atoul($1, &ulval) == 0) {
				if (ulval > 255) {
					yyerror("illegal icmp6-type %d", ulval);
					YYERROR;
				}
				$$ = ulval + 1;
			} else {
				if ((p = geticmptypebyname($1, AF_INET6)) ==
				    NULL) {
					yyerror("unknown icmp6-type %s", $1);
					YYERROR;
				}
				$$ = p->type + 1;
			}
		}
		;

tos		: TOS STRING			{
			if (!strcmp($2, "lowdelay"))
				$$ = IPTOS_LOWDELAY;
			else if (!strcmp($2, "throughput"))
				$$ = IPTOS_THROUGHPUT;
			else if (!strcmp($2, "reliability"))
				$$ = IPTOS_RELIABILITY;
			else if ($2[0] == '0' && $2[1] == 'x')
				$$ = strtoul($2, NULL, 16);
			else
				$$ = strtoul($2, NULL, 10);
			if (!$$ || $$ > 255) {
				yyerror("illegal tos value %s", $2);
				YYERROR;
			}
		}
		;

keep		: KEEP STATE state_opt_spec	{
			$$.action = PF_STATE_NORMAL;
			$$.options = $3;
		}
		| MODULATE STATE state_opt_spec	{
			$$.action = PF_STATE_MODULATE;
			$$.options = $3;
		}
		;

state_opt_spec	: '(' state_opt_list ')'	{ $$ = $2; }
		| /* empty */			{ $$ = NULL; }
		;

state_opt_list	: state_opt_item		{ $$ = $1; }
		| state_opt_list comma state_opt_item {
			$1->tail->next = $3;
			$1->tail = $3;
			$$ = $1;
		}
		;

state_opt_item	: MAXIMUM number		{
			$$ = calloc(1, sizeof(struct node_state_opt));
			if ($$ == NULL)
				err(1, "state_opt_item: calloc");
			$$->type = PF_STATE_OPT_MAX;
			$$->data.max_states = $2;
			$$->next = NULL;
			$$->tail = $$;
		}
		| STRING number			{
			int	i;

			for (i = 0; pf_timeouts[i].name &&
			    strcmp(pf_timeouts[i].name, $1); ++i)
				;	/* nothing */
			if (!pf_timeouts[i].name) {
				yyerror("illegal timeout name %s", $1);
				YYERROR;
			}
			if (strchr(pf_timeouts[i].name, '.') == NULL) {
				yyerror("illegal state timeout %s", $1);
				YYERROR;
			}
			$$ = calloc(1, sizeof(struct node_state_opt));
			if ($$ == NULL)
				err(1, "state_opt_item: calloc");
			$$->type = PF_STATE_OPT_TIMEOUT;
			$$->data.timeout.number = pf_timeouts[i].timeout;
			$$->data.timeout.seconds = $2;
			$$->next = NULL;
			$$->tail = $$;
		}
		;

label		: LABEL STRING			{
			if (($$ = strdup($2)) == NULL)
				err(1, "rule label strdup() failed");
		}
		;

qname		: QUEUE STRING				{
			if (($$.qname = strdup($2)) == NULL)
				err(1, "qname strdup() failed");
		}
		| QUEUE '(' STRING ')'			{
			if (($$.qname = strdup($3)) == NULL)
				err(1, "qname strdup() failed");
		}
		| QUEUE '(' STRING comma STRING ')'	{
			if (($$.qname = strdup($3)) == NULL ||
			    ($$.pqname = strdup($5)) == NULL)
				err(1, "qname strdup() failed");
		}
		;

no		: /* empty */			{ $$ = 0; }
		| NO				{ $$ = 1; }
		;

rport		: STRING			{
			char	*p = strchr($1, ':');

			if (p == NULL) {
				if (($$.a = getservice($1)) == -1)
					YYERROR;
				$$.b = $$.t = 0;
			} else if (!strcmp(p+1, "*")) {
				*p = 0;
				if (($$.a = getservice($1)) == -1)
					YYERROR;
				$$.b = 0;
				$$.t = 1;
			} else {
				*p++ = 0;
				if (($$.a = getservice($1)) == -1 ||
				    ($$.b = getservice(p)) == -1)
					YYERROR;
				if ($$.a == $$.b)
					$$.b = 0;
				$$.t = 0;
			}
		}
		;

redirspec	: host				{ $$ = $1; }
		| '{' redir_host_list '}'	{ $$ = $2; }
		;

redir_host_list	: host				{ $$ = $1; }
		| redir_host_list comma host	{
			/* $3 may be a list, so use its tail pointer */
			$1->tail->next = $3->tail;
			$1->tail = $3->tail;
			$$ = $1;
		}
		;

redirpool	: /* empty */			{ $$ = NULL; }
		| ARROW redirspec		{
			$$ = calloc(1, sizeof(struct redirection));
			if ($$ == NULL)
				err(1, "redirection: calloc");
			$$->host = $2;
			$$->rport.a = $$->rport.b = $$->rport.t = 0;
		}
		| ARROW redirspec PORT rport	{
			$$ = calloc(1, sizeof(struct redirection));
			if ($$ == NULL)
				err(1, "redirection: calloc");
			$$->host = $2;
			$$->rport = $4;
		}
		;

hashkey		: /* empty */
		{
			$$ = calloc(1, sizeof(struct pf_poolhashkey));
			if ($$ == NULL)
				err(1, "hashkey: calloc");
			$$->key32[0] = arc4random();
			$$->key32[1] = arc4random();
			$$->key32[2] = arc4random();
			$$->key32[3] = arc4random();
		}
		| string
		{
			if (!strncmp($1, "0x", 2)) {
				if (strlen($1) != 34) {
					yyerror("hex key must be 128 bits "
						"(32 hex digits) long");
					YYERROR;
				}
				$$ = calloc(1, sizeof(struct pf_poolhashkey));
				if ($$ == NULL)
					err(1, "hashkey: calloc");

				if (sscanf($1, "0x%8x%8x%8x%8x",
				    &$$->key32[0], &$$->key32[1],
				    &$$->key32[2], &$$->key32[3]) != 4) {
					free($$);
					yyerror("invalid hex key");
					YYERROR;
				}
			} else {
				MD5_CTX	context;

				$$ = calloc(1, sizeof(struct pf_poolhashkey));
				if ($$ == NULL)
					err(1, "hashkey: calloc");
				MD5Init(&context);
				MD5Update(&context, (unsigned char *)$1,
				    strlen($1));
				MD5Final((unsigned char *)$$, &context);
				HTONL($$->key32[0]);
				HTONL($$->key32[1]);
				HTONL($$->key32[2]);
				HTONL($$->key32[3]);
			}
		}
		;

pooltype	: /* empty */
		{
			$$.type = PF_POOL_NONE;
			$$.key = NULL;
		}
		| BITMASK
		{
			$$.type = PF_POOL_BITMASK;
			$$.key = NULL;
		}
		| RANDOM
		{
			$$.type = PF_POOL_RANDOM;
			$$.key = NULL;
		}
		| SOURCEHASH hashkey
		{
			$$.type = PF_POOL_SRCHASH;
			$$.key = $2;
		}
		| ROUNDROBIN
		{
			$$.type = PF_POOL_ROUNDROBIN;
			$$.key = NULL;
		}
		;

staticport	: /* empty */			{ $$ = 0; }
		| STATICPORT			{ $$ = PF_POOL_STATICPORT; }
		;

redirection	: /* empty */			{ $$ = NULL; }
		| ARROW host			{
			$$ = calloc(1, sizeof(struct redirection));
			if ($$ == NULL)
				err(1, "redirection: calloc");
			$$->host = $2;
			$$->rport.a = $$->rport.b = $$->rport.t = 0;
		}
		| ARROW host PORT rport	{
			$$ = calloc(1, sizeof(struct redirection));
			if ($$ == NULL)
				err(1, "redirection: calloc");
			$$->host = $2;
			$$->rport = $4;
		}
		;

nataction	: no NAT {
			$$.b2 = $$.w = 0;
			if ($1)
				$$.b1 = PF_NONAT;
			else
				$$.b1 = PF_NAT;
		}
		| no RDR {
			$$.b2 = $$.w = 0;
			if ($1)
				$$.b1 = PF_NORDR;
			else
				$$.b1 = PF_RDR;
		}
		;

natrule		: nataction interface af proto fromto redirpool pooltype
		  staticport
		{
			struct pf_rule	r;

			if (check_rulestate(PFCTL_STATE_NAT))
				YYERROR;

			memset(&r, 0, sizeof(r));

			r.action = $1.b1;
			r.af = $3;

			if (!r.af) {
				if ($5.src.host && $5.src.host->af &&
				    !$5.src.host->ifindex)
					r.af = $5.src.host->af;
				else if ($5.dst.host && $5.dst.host->af &&
				    !$5.dst.host->ifindex)
					r.af = $5.dst.host->af;
			}

			if (r.action == PF_NONAT || r.action == PF_NORDR) {
				if ($6 != NULL) {
					yyerror("translation rule with 'no' "
					    "does not need '->'");
					YYERROR;
				}
			} else {
				if ($6 == NULL || $6->host == NULL) {
					yyerror("translation rule requires '-> "
					    "address'");
					YYERROR;
				}
				if (disallow_table($6->host, "invalid use of "
				    "table <%s> as the redirection address "
				    "of a translation rule"))
					YYERROR;
				if (!r.af && ! $6->host->ifindex)
					r.af = $6->host->af;

				remove_invalid_hosts(&$6->host, &r.af);
				if (invalid_redirect($6->host, r.af))
					YYERROR;

				r.rpool.proxy_port[0] = ntohs($6->rport.a);

				switch (r.action) {
				case PF_RDR:
					if (!$6->rport.b && $6->rport.t &&
					    $5.dst.port != NULL) {
						r.rpool.proxy_port[1] =
						    ntohs($6->rport.a) +
						    (ntohs($5.dst.port->port[1]) -
						    ntohs($5.dst.port->port[0]));
					} else
						r.rpool.proxy_port[1] =
						    ntohs($6->rport.b);
					break;
				case PF_NAT:
					if (!r.rpool.proxy_port[0] &&
					    !r.rpool.proxy_port[1]) {
						r.rpool.proxy_port[0] =
						    PF_NAT_PROXY_PORT_LOW;
						r.rpool.proxy_port[1] =
						    PF_NAT_PROXY_PORT_HIGH;
					} else if (!r.rpool.proxy_port[1])
						r.rpool.proxy_port[1] =
						    ntohs(r.rpool.proxy_port[0]);
					break;
				default:
					break;
				}

				if ($6->host->next) {
					r.rpool.opts = $7.type;
					if (r.rpool.opts == PF_POOL_NONE)
						r.rpool.opts =
						    PF_POOL_ROUNDROBIN;
					if (r.rpool.opts !=
					    PF_POOL_ROUNDROBIN) {
						yyerror("only round-robin "
						    "valid for multiple "
						    "redirection addresses");
						YYERROR;
					}
				} else {
					if ((r.af == AF_INET &&
					    unmask(&$6->host->addr.v.a.mask,
					    r.af) == 32) ||
					    (r.af == AF_INET6 &&
					    unmask(&$6->host->addr.v.a.mask,
					    r.af) == 128)) {
						r.rpool.opts = PF_POOL_NONE;
					} else {
						if ($7.type == PF_POOL_NONE)
							r.rpool.opts =
							    PF_POOL_ROUNDROBIN;
						else
							r.rpool.opts =
							    $7.type;
					}
				}
			}

			if ($7.key != NULL)
				memcpy(&r.rpool.key, $7.key,
				    sizeof(struct pf_poolhashkey));

			if ($8 != NULL) {
				if (r.action == PF_NAT)
					r.rpool.opts |= PF_POOL_STATICPORT;
				else {
					yyerror("the 'static-port' option is "
					    "only valid with nat rules");
					YYERROR;
				}
			}

			expand_rule(&r, $2, $6 == NULL ? NULL : $6->host, $4,
			    $5.src.host, $5.src.port, $5.dst.host, $5.dst.port,
			    0, 0, 0);
			free($6);
		}
		;

binatrule	: no BINAT interface af proto FROM host TO ipspec redirection
		{
			struct pf_rule		binat;
			struct pf_pooladdr	*pa;

			if (check_rulestate(PFCTL_STATE_NAT))
				YYERROR;

			memset(&binat, 0, sizeof(binat));

			if ($1)
				binat.action = PF_NOBINAT;
			else
				binat.action = PF_BINAT;
			binat.af = $4;
			if (!binat.af && $7 != NULL && $7->af)
				binat.af = $7->af;
			if (!binat.af && $9 != NULL && $9->af)
				binat.af = $9->af;
			if (!binat.af && $10 != NULL && $10->host)
				binat.af = $10->host->af;
			if (!binat.af) {
				yyerror("address family (inet/inet6) "
				    "undefined");
				YYERROR;
			}

			if ($3 != NULL) {
				memcpy(binat.ifname, $3->ifname,
				    sizeof(binat.ifname));
				free($3);
			}
			if ($5 != NULL) {
				binat.proto = $5->proto;
				free($5);
			}

			if ($7 != NULL && disallow_table($7, "invalid use of "
			    "table <%s> as the source address of a binat rule"))
				YYERROR;
			if ($10 != NULL && $10->host != NULL && disallow_table(
			    $10->host, "invalid use of table <%s> as the "
			    "redirect address of a binat rule"))
				YYERROR;

			if ($7 != NULL) {
				if ($7->next) {
					yyerror("multiple binat ip addresses");
					YYERROR;
				}
				if ($7->addr.type == PF_ADDR_DYNIFTL)
					$7->af = binat.af;
				if ($7->af != binat.af) {
					yyerror("binat ip versions must match");
					YYERROR;
				}
				memcpy(&binat.src.addr, &$7->addr,
				    sizeof(binat.src.addr));
				free($7);
			}
			if ($9 != NULL) {
				if ($9->next) {
					yyerror("multiple binat ip addresses");
					YYERROR;
				}
				if ($9->af != binat.af && $9->af) {
					yyerror("binat ip versions must match");
					YYERROR;
				}
				memcpy(&binat.dst.addr, &$9->addr,
				    sizeof(binat.dst.addr));
				binat.dst.not = $9->not;
				free($9);
			}

			if (binat.action == PF_NOBINAT) {
				if ($10 != NULL) {
					yyerror("'no binat' rule does not need"
					    " '->'");
					YYERROR;
				}
			} else {
				if ($10 == NULL || $10->host == NULL) {
					yyerror("'binat' rule requires"
					    " '-> address'");
					YYERROR;
				}

				remove_invalid_hosts(&$10->host, &binat.af);
				if (invalid_redirect($10->host, binat.af))
					YYERROR;
				if ($10->host->next != NULL) {
					yyerror("binat rule must redirect to "
					    "a single address");
					YYERROR;
				}

				if (!PF_AZERO(&binat.src.addr.v.a.mask,
				    binat.af) &&
				    !PF_AEQ(&binat.src.addr.v.a.mask,
				    &$10->host->addr.v.a.mask, binat.af)) {
					yyerror("'binat' source mask and "
					    "redirect mask must be the same");
					YYERROR;
				}

				TAILQ_INIT(&binat.rpool.list);
				pa = calloc(1, sizeof(struct pf_pooladdr));
				if (pa == NULL)
					err(1, "binat: calloc");
				pa->addr.addr = $10->host->addr;
				pa->ifname[0] = 0;
				TAILQ_INSERT_TAIL(&binat.rpool.list,
				    pa, entries);

				free($10);
			}

			pfctl_add_rule(pf, &binat);
		}
		;

route_host	: STRING			{
			struct node_host	*n;

			$$ = calloc(1, sizeof(struct node_host));
			if ($$ == NULL)
				err(1, "route_host: calloc");
			if (($$->ifname = strdup($1)) == NULL)
				err(1, "routeto: strdup");
			if ((n = ifa_exists($$->ifname)) == NULL) {
				yyerror("routeto: unknown interface %s",
				    $$->ifname);
				YYERROR;
			}
			set_ipmask($$, 128);
			$$->next = NULL;
			$$->tail = $$;
		}
		| '(' STRING host ')'		{
			struct node_host	*n;

			$$ = $3;
			if (($$->ifname = strdup($2)) == NULL)
				err(1, "routeto: strdup");
			if ((n = ifa_exists($$->ifname)) == NULL) {
				yyerror("routeto: unknown interface %s",
				    $$->ifname);
				YYERROR;
			}
			if (disallow_table($3, "invalid use of table <%s> in "
			    "a route expression"))
				YYERROR;
		}
		;

route_host_list	: route_host				{ $$ = $1; }
		| route_host_list comma route_host	{
			if ($1->af == 0)
				$1->af = $3->af;
			if ($1->af != $3->af) {
				yyerror("all pool addresses must be in the "
				    "same address family");
				YYERROR;
			}
			/* $3 may be a list, so use its tail pointer */
			$1->tail->next = $3->tail;
			$1->tail = $3->tail;
			$$ = $1;
		}
		;

routespec	: route_host			{ $$ = $1; }
		| '{' route_host_list '}'	{ $$ = $2; }
		;

route		: /* empty */			{
			$$.host = NULL;
			$$.rt = 0;
			$$.pool_opts = 0;
		}
		| FASTROUTE {
			$$.host = NULL;
			$$.rt = PF_FASTROUTE;
			$$.pool_opts = 0;
		}
		| ROUTETO routespec pooltype {
			$$.host = $2;
			$$.rt = PF_ROUTETO;
			$$.pool_opts = $3.type;
			if ($3.key != NULL)
				$$.key = $3.key;
		}
		| REPLYTO routespec pooltype {
			$$.host = $2;
			$$.rt = PF_REPLYTO;
			$$.pool_opts = $3.type;
			if ($3.key != NULL)
				$$.key = $3.key;
		}
		| DUPTO routespec pooltype {
			$$.host = $2;
			$$.rt = PF_DUPTO;
			$$.pool_opts = $3.type;
			if ($3.key != NULL)
				$$.key = $3.key;
		}
		;

timeout_spec	: STRING number
		{
			if (check_rulestate(PFCTL_STATE_OPTION))
				YYERROR;
			if (pfctl_set_timeout(pf, $1, $2, 0) != 0) {
				yyerror("unknown timeout %s", $1);
				YYERROR;
			}
		}
		;

timeout_list	: timeout_list comma timeout_spec
		| timeout_spec
		;

limit_spec	: STRING number
		{
			if (check_rulestate(PFCTL_STATE_OPTION))
				YYERROR;
			if (pfctl_set_limit(pf, $1, $2) != 0) {
				yyerror("unable to set limit %s %u", $1, $2);
				YYERROR;
			}
		}

limit_list	: limit_list comma limit_spec
		| limit_spec
		;

comma		: ','
		| /* empty */
		;

yesno		: NO			{ $$ = 0; }
		| STRING		{
			if (!strcmp($1, "yes"))
				$$ = 1;
			else
				YYERROR;
		}

%%

int
yyerror(char *fmt, ...)
{
	va_list		 ap;
	extern char	*infile;

	errors = 1;
	va_start(ap, fmt);
	fprintf(stderr, "%s:%d: ", infile, yylval.lineno);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	return (0);
}

int
disallow_table(struct node_host *h, char *fmt)
{
	for (; h != NULL; h = h->next)
		if (h->addr.type == PF_ADDR_TABLE) {
			yyerror(fmt, h->addr.v.tblname);
			return (1);
		}
	return (0);
}

int
rule_consistent(struct pf_rule *r)
{
	int	problems = 0;
	switch (r->action) {
	case PF_PASS:
	case PF_DROP:
	case PF_SCRUB:
		problems = filter_consistent(r);
		break;
	case PF_NAT:
	case PF_NONAT:
		problems = nat_consistent(r);
		break;
	case PF_RDR:
	case PF_NORDR:
		problems = rdr_consistent(r);
		break;
	case PF_BINAT:
	case PF_NOBINAT:
	default:
		break;
	}
	return (problems);
}

int
filter_consistent(struct pf_rule *r)
{
	int	problems = 0;

	if (r->proto != IPPROTO_TCP && r->proto != IPPROTO_UDP &&
	    (r->src.port_op || r->dst.port_op)) {
		yyerror("port only applies to tcp/udp");
		problems++;
	}
	if (r->src.port_op == PF_OP_RRG || r->dst.port_op == PF_OP_RRG) {
		yyerror("the ':' port operator only applies to rdr");
		problems++;
	}
	if (r->proto != IPPROTO_ICMP && r->proto != IPPROTO_ICMPV6 &&
	    (r->type || r->code)) {
		yyerror("icmp-type/code only applies to icmp");
		problems++;
	}
	if (!r->af && (r->type || r->code)) {
		yyerror("must indicate address family with icmp-type/code");
		problems++;
	}
	if ((r->proto == IPPROTO_ICMP && r->af == AF_INET6) ||
	    (r->proto == IPPROTO_ICMPV6 && r->af == AF_INET)) {
		yyerror("proto %s doesn't match address family %s",
		    r->proto == IPPROTO_ICMP ? "icmp" : "icmp6",
		    r->af == AF_INET ? "inet" : "inet6");
		problems++;
	}
	if (r->keep_state == PF_STATE_MODULATE && r->proto &&
	    r->proto != IPPROTO_TCP) {
		yyerror("modulate state can only be applied to TCP rules");
		problems++;
	}
	if (r->allow_opts && r->action != PF_PASS) {
		yyerror("allow-opts can only be specified for pass rules");
		problems++;
	}
	if (!r->af && (r->src.addr.type == PF_ADDR_DYNIFTL ||
	    r->dst.addr.type == PF_ADDR_DYNIFTL)) {
		yyerror("dynamic addresses require address family "
		    "(inet/inet6)");
		problems++;
	}
	if (r->rule_flag & PFRULE_FRAGMENT && (r->src.port_op ||
	    r->dst.port_op || r->flagset || r->type || r->code)) {
		yyerror("fragments can be filtered only on IP header fields");
		problems++;
	}
	if (r->rule_flag & PFRULE_RETURNRST && r->proto != IPPROTO_TCP) {
		yyerror("return-rst can only be applied to TCP rules");
		problems++;
	}
	if (r->action == PF_DROP && r->keep_state) {
		yyerror("keep state on block rules doesn't make sense");
		problems++;
	}
	return (-problems);
}

int
nat_consistent(struct pf_rule *r)
{
	int			 problems = 0;
	struct pf_pooladdr	*pa;

	if (r->src.port_op == PF_OP_RRG || r->dst.port_op == PF_OP_RRG) {
		yyerror("the ':' port operator only applies to rdr");
		problems++;
	}
	if (!r->af) {
		TAILQ_FOREACH(pa, &r->rpool.list, entries) {
			if (pa->addr.addr.type == PF_ADDR_DYNIFTL) {
				yyerror("dynamic addresses require "
				    "address family (inet/inet6)");
				problems++;
				break;
			}
		}
	}
	return (-problems);
}

int
rdr_consistent(struct pf_rule *r)
{
	int			 problems = 0;
	struct pf_pooladdr	*pa;

	if (r->proto != IPPROTO_TCP && r->proto != IPPROTO_UDP) {
		if (r->src.port_op) {
			yyerror("src port only applies to tcp/udp");
			problems++;
		}
		if (r->dst.port_op) {
			yyerror("dst port only applies to tcp/udp");
			problems++;
		}
		if (r->rpool.proxy_port[0]) {
			yyerror("rpool port only applies to tcp/udp");
			problems++;
		}
	}
	if (r->dst.port_op &&
	    r->dst.port_op != PF_OP_EQ && r->dst.port_op != PF_OP_RRG) {
		yyerror("invalid port operator for rdr destination port");
		problems++;
	}
	if (r->src.port_op == PF_OP_RRG) {
		yyerror("the ':' port operator only applies to rdr "
		    "destination port");
		problems++;
	}
	if (!r->af) {
		if (r->src.addr.type == PF_ADDR_DYNIFTL ||
		    r->dst.addr.type == PF_ADDR_DYNIFTL) {
			yyerror("dynamic addresses require address family "
			    "(inet/inet6)");
			problems++;
		} else {
			TAILQ_FOREACH(pa, &r->rpool.list, entries) {
				if (pa->addr.addr.type == PF_ADDR_DYNIFTL) {
					yyerror("dynamic addresses require "
					    "address family (inet/inet6)");
					problems++;
					break;
				}
			}
		}
	}
	return (-problems);
}

struct keywords {
	const char	*k_name;
	int		 k_val;
};

/* macro gore, but you should've seen the prior indentation nightmare... */

#define FREE_LIST(T,r) \
	do { \
		T *p, *node = r; \
		while (node != NULL) { \
			p = node; \
			node = node->next; \
			free(p); \
		} \
	} while (0)

#define LOOP_THROUGH(T,n,r,C) \
	do { \
		T *n; \
		if (r == NULL) { \
			r = calloc(1, sizeof(T)); \
			if (r == NULL) \
				err(1, "LOOP: calloc"); \
			r->next = NULL; \
		} \
		n = r; \
		while (n != NULL) { \
			do { \
				C; \
			} while (0); \
			n = n->next; \
		} \
	} while (0)

void
expand_label_str(char *label, const char *srch, const char *repl)
{
	char tmp[PF_RULE_LABEL_SIZE] = "";
	char *p, *q;

	p = q = label;
	while ((q = strstr(p, srch)) != NULL) {
		*q = '\0';
		if ((strlcat(tmp, p, sizeof(tmp)) >= sizeof(tmp)) ||
		    (strlcat(tmp, repl, sizeof(tmp)) >= sizeof(tmp)))
			err(1, "expand_label: label too long");
		q += strlen(srch);
		p = q;
	}
	if (strlcat(tmp, p, sizeof(tmp)) >= sizeof(tmp))
		err(1, "expand_label: label too long");
	strlcpy(label, tmp, PF_RULE_LABEL_SIZE);	/* always fits */
}

void
expand_label_if(const char *name, char *label, const char *ifname)
{
	if (strstr(label, name) != NULL) {
		if (!*ifname)
			expand_label_str(label, name, "any");
		else
			expand_label_str(label, name, ifname);
	}
}

void
expand_label_addr(const char *name, char *label, sa_family_t af,
    struct node_host *h)
{
	char tmp[64], tmp_not[66];

	if (strstr(label, name) != NULL) {
		if (h->addr.type == PF_ADDR_DYNIFTL)
			snprintf(tmp, sizeof(tmp), "(%s)", h->addr.v.ifname);
		else if (!af || (PF_AZERO(&h->addr.v.a.addr, af) &&
		    PF_AZERO(&h->addr.v.a.mask, af)))
			snprintf(tmp, sizeof(tmp), "any");
		else {
			char	a[48];
			int	bits;

			if (inet_ntop(af, &h->addr.v.a.addr, a, sizeof(a)) ==
			    NULL)
				snprintf(tmp, sizeof(tmp), "?");
			else {
				bits = unmask(&h->addr.v.a.mask, af);
				if ((af == AF_INET && bits < 32) ||
				    (af == AF_INET6 && bits < 128))
					snprintf(tmp, sizeof(tmp), "%s/%d",
					    a, bits);
				else
					snprintf(tmp, sizeof(tmp), "%s", a);
			}
		}

		if (h->not) {
			snprintf(tmp_not, sizeof(tmp_not), "! %s", tmp);
			expand_label_str(label, name, tmp_not);
		} else
			expand_label_str(label, name, tmp);
	}
}

void
expand_label_port(const char *name, char *label, struct node_port *port)
{
	char	 a1[6], a2[6], op[13] = "";

	if (strstr(label, name) != NULL) {
		snprintf(a1, sizeof(a1), "%u", ntohs(port->port[0]));
		snprintf(a2, sizeof(a2), "%u", ntohs(port->port[1]));
		if (!port->op)
			;
		else if (port->op == PF_OP_IRG)
			snprintf(op, sizeof(op), "%s><%s", a1, a2);
		else if (port->op == PF_OP_XRG)
			snprintf(op, sizeof(op), "%s<>%s", a1, a2);
		else if (port->op == PF_OP_EQ)
			snprintf(op, sizeof(op), "%s", a1);
		else if (port->op == PF_OP_NE)
			snprintf(op, sizeof(op), "!=%s", a1);
		else if (port->op == PF_OP_LT)
			snprintf(op, sizeof(op), "<%s", a1);
		else if (port->op == PF_OP_LE)
			snprintf(op, sizeof(op), "<=%s", a1);
		else if (port->op == PF_OP_GT)
			snprintf(op, sizeof(op), ">%s", a1);
		else if (port->op == PF_OP_GE)
			snprintf(op, sizeof(op), ">=%s", a1);
		expand_label_str(label, name, op);
	}
}

void
expand_label_proto(const char *name, char *label, u_int8_t proto)
{
	struct protoent *pe;
	char n[4];

	if (strstr(label, name) != NULL) {
		pe = getprotobynumber(proto);
		if (pe != NULL)
			expand_label_str(label, name, pe->p_name);
		else {
			snprintf(n, sizeof(n), "%u", proto);
			expand_label_str(label, name, n);
		}
	}
}

void
expand_label_nr(const char *name, char *label)
{
	char n[11];

	if (strstr(label, name) != NULL) {
		snprintf(n, sizeof(n), "%u", pf->rule_nr);
		expand_label_str(label, name, n);
	}
}

void
expand_label(char *label, const char *ifname, sa_family_t af,
    struct node_host *src_host, struct node_port *src_port,
    struct node_host *dst_host, struct node_port *dst_port,
    u_int8_t proto)
{
	expand_label_if("$if", label, ifname);
	expand_label_addr("$srcaddr", label, af, src_host);
	expand_label_addr("$dstaddr", label, af, dst_host);
	expand_label_port("$srcport", label, src_port);
	expand_label_port("$dstport", label, dst_port);
	expand_label_proto("$proto", label, proto);
	expand_label_nr("$nr", label);
}

int
expand_altq(struct pf_altq *a, struct node_if *interfaces,
    struct node_queue *nqueues, struct node_queue_bw bwspec)
{
	struct pf_altq		 pa, pb;
	char			 qname[PF_QNAME_SIZE];
	struct node_queue	*n;
	int			 errs = 0;

	if ((pf->loadopt & (PFCTL_FLAG_ALTQ | PFCTL_FLAG_ALL)) == 0) {
		FREE_LIST(struct node_if, interfaces);
		FREE_LIST(struct node_queue, nqueues);
		return (0);
	}

	LOOP_THROUGH(struct node_if, interface, interfaces,
		memcpy(&pa, a, sizeof(struct pf_altq));
		if (strlcpy(pa.ifname, interface->ifname,
		    sizeof(pa.ifname)) >= sizeof(pa.ifname))
			errx(1, "expand_altq: strlcpy");

		if (interface->not) {
			yyerror("altq on ! <interface> is not supported");
			errs++;
		} else {
			if (eval_pfaltq(pf, &pa, bwspec.bw_absolute,
			    bwspec.bw_percent))
				errs++;
			else
				if (pfctl_add_altq(pf, &pa))
					errs++;

			if (pf->opts & PF_OPT_VERBOSE) {
				print_altq(&pf->paltq->altq, 0,
				    bwspec.bw_percent);
				if (nqueues && nqueues->tail) {
					printf("queue { ");
					LOOP_THROUGH(struct node_queue, queue,
					    nqueues,
						printf("%s ",
						    queue->queue);
					);
					printf("}");
				}
				printf("\n");
			}

			if (pa.scheduler == ALTQT_CBQ) {
				/* now create a root queue */
				memset(&pb, 0, sizeof(struct pf_altq));
				if (strlcpy(qname, "root_", sizeof(qname)) >=
				    sizeof(qname))
					errx(1, "expand_altq: strlcpy");
				if (strlcat(qname, interface->ifname,
				    sizeof(qname)) >= sizeof(qname))
					errx(1, "expand_altq: strlcat");
				if (strlcpy(pb.qname, qname,
				    sizeof(pb.qname)) >= sizeof(pb.qname))
					errx(1, "expand_altq: strlcpy");
				if (strlcpy(pb.ifname, interface->ifname,
				    sizeof(pb.ifname)) >= sizeof(pb.ifname))
					errx(1, "expand_altq: strlcpy");
				pb.qlimit = pa.qlimit;
				pb.scheduler = pa.scheduler;
				pb.pq_u.cbq_opts = pa.pq_u.cbq_opts;
				if (eval_pfqueue(pf, &pb, pa.ifbandwidth, 0))
					errs++;
				else
					if (pfctl_add_altq(pf, &pb))
						errs++;
			}

			LOOP_THROUGH(struct node_queue, queue, nqueues,
				n = calloc(1, sizeof(struct node_queue));
				if (n == NULL)
					err(1, "expand_altq: calloc");
				if (pa.scheduler == ALTQT_CBQ)
					if (strlcpy(n->parent, qname,
					    sizeof(n->parent)) >=
					    sizeof(n->parent))
						errx(1, "expand_altq: strlcpy");
				if (strlcpy(n->queue, queue->queue,
				    sizeof(n->queue)) >= sizeof(n->queue))
					errx(1, "expand_altq: strlcpy");
				if (strlcpy(n->ifname, interface->ifname,
				    sizeof(n->ifname)) >= sizeof(n->ifname))
					errx(1, "expand_altq: strlcpy");
				n->scheduler = pa.scheduler;
				n->next = NULL;
				n->tail = n;
				if (queues == NULL)
					queues = n;
				else {
					queues->tail->next = n;
					queues->tail = n;
				}
			);
		}
	);
	FREE_LIST(struct node_if, interfaces);
	FREE_LIST(struct node_queue, nqueues);

	return (errs);
}

int
expand_queue(struct pf_altq *a, struct node_queue *nqueues,
    struct node_queue_bw bwspec)
{
	struct node_queue	*n;
	struct pf_altq		 pa;
	u_int8_t		 found = 0;
	u_int8_t		 errs = 0;

	if ((pf->loadopt & (PFCTL_FLAG_ALTQ | PFCTL_FLAG_ALL)) == 0) {
		FREE_LIST(struct node_queue, nqueues);
		return (0);
	}

	if (queues == NULL) {
		yyerror("queue %s has no parent", a->qname);
		FREE_LIST(struct node_queue, nqueues);
		return (1);
	}

	LOOP_THROUGH(struct node_queue, tqueue, queues,
		if (!strncmp(a->qname, tqueue->queue, PF_QNAME_SIZE)) {
			/* found ourselve in queues */
			found++;

			memcpy(&pa, a, sizeof(struct pf_altq));

			if (pa.scheduler != ALTQT_NONE &&
			    pa.scheduler != tqueue->scheduler) {
				yyerror("exactly one scheduler type per "
				    "interface allowed");
				return (1);
			}
			pa.scheduler = tqueue->scheduler;

			/* scheduler dependent error checking */
			switch (pa.scheduler) {
			case ALTQT_PRIQ:
				if (nqueues != NULL) {
					yyerror("priq queues cannot have "
					    "child queues");
					return (1);
				}
				if (bwspec.bw_absolute > 0 ||
				    bwspec.bw_percent < 100) {
					yyerror("priq doesn't take bandwidth");
					return (1);
				}
				break;
			default:
				break;
			}

			if (strlcpy(pa.ifname, tqueue->ifname,
			    sizeof(pa.ifname)) >= sizeof(pa.ifname))
				errx(1, "expand_queue: strlcpy");
			if (strlcpy(pa.parent, tqueue->parent,
			    sizeof(pa.parent)) >= sizeof(pa.parent))
				errx(1, "expand_queue: strlcpy");

			if (eval_pfqueue(pf, &pa, bwspec.bw_absolute,
			    bwspec.bw_percent))
				errs++;
			else
				if (pfctl_add_altq(pf, &pa))
					errs++;

			if (nqueues == NULL)
				continue;

			LOOP_THROUGH(struct node_queue, queue, nqueues,
				n = calloc(1, sizeof(struct node_queue));
				if (n == NULL)
					err(1, "expand_queue: calloc");
				if (strlcpy(n->parent, a->qname,
				    sizeof(n->parent)) >= sizeof(n->parent))
					errx(1, "expand_queue: strlcpy");
				if (strlcpy(n->queue, queue->queue,
				    sizeof(n->queue)) >= sizeof(n->queue))
					errx(1, "expand_queue: strlcpy");
				if (strlcpy(n->ifname, tqueue->ifname,
				    sizeof(n->ifname)) >= sizeof(n->ifname))
					errx(1, "expand_queue: strlcpy");
				n->scheduler = tqueue->scheduler;
				n->next = NULL;
				n->tail = n;
				if (queues == NULL)
					queues = n;
				else {
					queues->tail->next = n;
					queues->tail = n;
				}
			);
		}
	);

	if ((pf->opts & PF_OPT_VERBOSE) && found > 0) {
		print_altq(&pf->paltq->altq, 0, bwspec.bw_percent);
		if (nqueues && nqueues->tail) {
			printf("{ ");
			LOOP_THROUGH(struct node_queue, queue,
			    nqueues,
				printf("%s ", queue->queue);
			);
			printf("}");
		}
		printf("\n");
	}

	FREE_LIST(struct node_queue, nqueues);

	if (!found) {
		yyerror("queue %s has no parent", a->qname);
		errs++;
	}

	if (errs)
		return (1);
	else
		return (0);
}

void
expand_rule(struct pf_rule *r,
    struct node_if *interfaces, struct node_host *rpool_hosts,
    struct node_proto *protos, struct node_host *src_hosts,
    struct node_port *src_ports, struct node_host *dst_hosts,
    struct node_port *dst_ports, struct node_uid *uids,
    struct node_gid *gids, struct node_icmp *icmp_types)
{
	sa_family_t		 af = r->af;
	int			 added = 0, error = 0;
	char			 ifname[IF_NAMESIZE];
	char			 label[PF_RULE_LABEL_SIZE];
	struct pf_pooladdr	*pa;
	struct node_host	*h;
	u_int8_t		 flags, flagset;

	if (strlcpy(label, r->label, sizeof(label)) >= sizeof(label))
		errx(1, "expand_rule: strlcpy");
	flags = r->flags;
	flagset = r->flagset;

	LOOP_THROUGH(struct node_if, interface, interfaces,
	LOOP_THROUGH(struct node_proto, proto, protos,
	LOOP_THROUGH(struct node_icmp, icmp_type, icmp_types,
	LOOP_THROUGH(struct node_host, src_host, src_hosts,
	LOOP_THROUGH(struct node_port, src_port, src_ports,
	LOOP_THROUGH(struct node_host, dst_host, dst_hosts,
	LOOP_THROUGH(struct node_port, dst_port, dst_ports,
	LOOP_THROUGH(struct node_uid, uid, uids,
	LOOP_THROUGH(struct node_gid, gid, gids,

		r->af = af;
		/* for link-local IPv6 address, interface must match up */
		if ((r->af && src_host->af && r->af != src_host->af) ||
		    (r->af && dst_host->af && r->af != dst_host->af) ||
		    (src_host->af && dst_host->af &&
		    src_host->af != dst_host->af) ||
		    (src_host->ifindex && dst_host->ifindex &&
		    src_host->ifindex != dst_host->ifindex) ||
		    (src_host->ifindex && if_nametoindex(interface->ifname) &&
		    src_host->ifindex != if_nametoindex(interface->ifname)) ||
		    (dst_host->ifindex && if_nametoindex(interface->ifname) &&
		    dst_host->ifindex != if_nametoindex(interface->ifname)))
			continue;
		if (!r->af && src_host->af)
			r->af = src_host->af;
		else if (!r->af && dst_host->af)
			r->af = dst_host->af;

		if (if_indextoname(src_host->ifindex, ifname))
			memcpy(r->ifname, ifname, sizeof(r->ifname));
		else if (if_indextoname(dst_host->ifindex, ifname))
			memcpy(r->ifname, ifname, sizeof(r->ifname));
		else
			memcpy(r->ifname, interface->ifname, sizeof(r->ifname));

		if (strlcpy(r->label, label, sizeof(r->label)) >=
		    sizeof(r->label))
			errx(1, "expand_rule: strlcpy");
		expand_label(r->label, r->ifname, r->af, src_host, src_port,
		    dst_host, dst_port, proto->proto);
		r->qid = qname_to_qid(r->qname);
		if ((r->pqid = qname_to_qid(r->pqname)) == 0)
			r->pqid = r->qid;
		r->ifnot = interface->not;
		r->proto = proto->proto;
		r->src.addr = src_host->addr;
		r->src.not = src_host->not;
		r->src.port[0] = src_port->port[0];
		r->src.port[1] = src_port->port[1];
		r->src.port_op = src_port->op;
		r->dst.addr = dst_host->addr;
		r->dst.not = dst_host->not;
		r->dst.port[0] = dst_port->port[0];
		r->dst.port[1] = dst_port->port[1];
		r->dst.port_op = dst_port->op;
		r->uid.op = uid->op;
		r->uid.uid[0] = uid->uid[0];
		r->uid.uid[1] = uid->uid[1];
		r->gid.op = gid->op;
		r->gid.gid[0] = gid->gid[0];
		r->gid.gid[1] = gid->gid[1];
		r->type = icmp_type->type;
		r->code = icmp_type->code;

		if (r->proto && r->proto != IPPROTO_TCP) {
			r->flags = 0;
			r->flagset = 0;
		} else {
			r->flags = flags;
			r->flagset = flagset;
		}
		if (icmp_type->proto && r->proto != icmp_type->proto) {
			yyerror("icmp-type mismatch");
			error++;
		}

		TAILQ_INIT(&r->rpool.list);
		for (h = rpool_hosts; h != NULL; h = h->next) {
			pa = calloc(1, sizeof(struct pf_pooladdr));
			if (pa == NULL)
				err(1, "expand_rule: calloc");
			pa->addr.addr = h->addr;
			if (h->ifname != NULL) {
				if (strlcpy(pa->ifname, h->ifname,
				    sizeof(pa->ifname)) >=
				    sizeof(pa->ifname))
					errx(1, "expand_rule: strlcpy");
			} else
				pa->ifname[0] = 0;
			TAILQ_INSERT_TAIL(&r->rpool.list, pa, entries);
		}

		if (rule_consistent(r) < 0 || error)
			yyerror("skipping rule due to errors");
		else {
			r->nr = pf->rule_nr++;
			pfctl_add_rule(pf, r);
			added++;
		}

	)))))))));

	FREE_LIST(struct node_if, interfaces);
	FREE_LIST(struct node_proto, protos);
	FREE_LIST(struct node_host, src_hosts);
	FREE_LIST(struct node_port, src_ports);
	FREE_LIST(struct node_host, dst_hosts);
	FREE_LIST(struct node_port, dst_ports);
	FREE_LIST(struct node_uid, uids);
	FREE_LIST(struct node_gid, gids);
	FREE_LIST(struct node_icmp, icmp_types);
	FREE_LIST(struct node_host, rpool_hosts);

	if (!added)
		yyerror("rule expands to no valid combination");
}

#undef FREE_LIST
#undef LOOP_THROUGH

int
check_rulestate(int desired_state)
{
	if (require_order && (rulestate > desired_state)) {
		yyerror("Rules must be in order: options, normalization, "
		    "queueing, translation, filtering");
		return (1);
	}
	rulestate = desired_state;
	return (0);
}

int
kw_cmp(const void *k, const void *e)
{
	return (strcmp(k, ((const struct keywords *)e)->k_name));
}

int
lookup(char *s)
{
	/* this has to be sorted always */
	static const struct keywords keywords[] = {
		{ "all",		ALL},
		{ "allow-opts",		ALLOWOPTS},
		{ "altq",		ALTQ},
		{ "anchor",		ANCHOR},
		{ "antispoof",		ANTISPOOF},
		{ "any",		ANY},
		{ "bandwidth",		BANDWIDTH},
		{ "binat",		BINAT},
		{ "binat-anchor",	BINATANCHOR},
		{ "bitmask",		BITMASK},
		{ "block",		BLOCK},
		{ "block-policy",	BLOCKPOLICY},
		{ "cbq",		CBQ},
		{ "code",		CODE},
		{ "crop",		FRAGCROP},
		{ "drop",		DROP},
		{ "drop-ovl",		FRAGDROP},
		{ "dup-to",		DUPTO},
		{ "fastroute",		FASTROUTE},
		{ "file",		FILENAME},
		{ "flags",		FLAGS},
		{ "for",		FOR},
		{ "fragment",		FRAGMENT},
		{ "from",		FROM},
		{ "group",		GROUP},
		{ "icmp-type",		ICMPTYPE},
		{ "icmp6-type",		ICMP6TYPE},
		{ "in",			IN},
		{ "inet",		INET},
		{ "inet6",		INET6},
		{ "keep",		KEEP},
		{ "label",		LABEL},
		{ "limit",		LIMIT},
		{ "log",		LOG},
		{ "log-all",		LOGALL},
		{ "loginterface",	LOGINTERFACE},
		{ "max",		MAXIMUM},
		{ "max-mss",		MAXMSS},
		{ "min-ttl",		MINTTL},
		{ "modulate",		MODULATE},
		{ "nat",		NAT},
		{ "nat-anchor",		NATANCHOR},
		{ "no",			NO},
		{ "no-df",		NODF},
		{ "no-route",		NOROUTE},
		{ "on",			ON},
		{ "optimization",	OPTIMIZATION},
		{ "out",		OUT},
		{ "pass",		PASS},
		{ "port",		PORT},
		{ "priority",		PRIORITY},
		{ "priq",		PRIQ},
		{ "proto",		PROTO},
		{ "qlimit",		QLIMIT},
		{ "queue",		QUEUE},
		{ "quick",		QUICK},
		{ "random",		RANDOM},
		{ "random-id",		RANDOMID},
		{ "rdr",		RDR},
		{ "rdr-anchor",		RDRANCHOR},
		{ "reassemble",		FRAGNORM},
		{ "reply-to",		REPLYTO},
		{ "require-order",	REQUIREORDER},
		{ "return",		RETURN},
		{ "return-icmp",	RETURNICMP},
		{ "return-icmp6",	RETURNICMP6},
		{ "return-rst",		RETURNRST},
		{ "round-robin",	ROUNDROBIN},
		{ "route-to",		ROUTETO},
		{ "scrub",		SCRUB},
		{ "set",		SET},
		{ "source-hash",	SOURCEHASH},
		{ "state",		STATE},
		{ "static-port",	STATICPORT},
		{ "table",		TABLE},
		{ "tbrsize",		TBRSIZE},
		{ "timeout",		TIMEOUT},
		{ "to",			TO},
		{ "tos",		TOS},
		{ "ttl",		TTL},
		{ "user",		USER},
	};
	const struct keywords	*p;

	p = bsearch(s, keywords, sizeof(keywords)/sizeof(keywords[0]),
	    sizeof(keywords[0]), kw_cmp);

	if (p) {
		if (debug > 1)
			fprintf(stderr, "%s: %d\n", s, p->k_val);
		return (p->k_val);
	} else {
		if (debug > 1)
			fprintf(stderr, "string: %s\n", s);
		return (STRING);
	}
}

#define MAXPUSHBACK	128

char	*parsebuf;
int	 parseindex;
char	 pushback_buffer[MAXPUSHBACK];
int	 pushback_index = 0;

int
lgetc(FILE *f)
{
	int	c, next;

	if (parsebuf) {
		/* Read character from the parsebuffer instead of input. */
		if (parseindex >= 0) {
			c = parsebuf[parseindex++];
			if (c != '\0')
				return (c);
			parsebuf = NULL;
		} else
			parseindex++;
	}

	if (pushback_index)
		return (pushback_buffer[--pushback_index]);

	while ((c = getc(f)) == '\\') {
		next = getc(f);
		if (next != '\n') {
			if (isspace(next))
				yyerror("whitespace after \\");
			ungetc(next, f);
			break;
		}
		yylval.lineno = lineno;
		lineno++;
	}
	if (c == '\t' || c == ' ') {
		/* Compress blanks to a single space. */
		do {
			c = getc(f);
		} while (c == '\t' || c == ' ');
		ungetc(c, f);
		c = ' ';
	}

	return (c);
}

int
lungetc(int c)
{
	if (c == EOF)
		return (EOF);
	if (parsebuf) {
		parseindex--;
		if (parseindex >= 0)
			return (c);
	}
	if (pushback_index < MAXPUSHBACK-1)
		return (pushback_buffer[pushback_index++] = c);
	else
		return (EOF);
}

int
findeol(void)
{
	int	c;

	parsebuf = NULL;
	pushback_index = 0;

	/* skip to either EOF or the first real EOL */
	while (1) {
		c = lgetc(fin);
		if (c == '\n') {
			lineno++;
			break;
		}
		if (c == EOF)
			break;
	}
	return (ERROR);
}

int
yylex(void)
{
	char	 buf[8096];
	char	*p, *val;
	int	 endc, c, next;
	int	 token;

top:
	p = buf;
	while ((c = lgetc(fin)) == ' ')
		; /* nothing */

	yylval.lineno = lineno;
	if (c == '#')
		while ((c = lgetc(fin)) != '\n' && c != EOF)
			; /* nothing */
	if (c == '$' && parsebuf == NULL) {
		while (1) {
			if ((c = lgetc(fin)) == EOF)
				return (0);

			if (p + 1 >= buf + sizeof(buf) - 1) {
				yyerror("string too long");
				return (findeol());
			}
			if (isalnum(c) || c == '_') {
				*p++ = (char)c;
				continue;
			}
			*p = '\0';
			lungetc(c);
			break;
		}
		val = symget(buf);
		if (val == NULL) {
			yyerror("macro '%s' not defined", buf);
			return (findeol());
		}
		parsebuf = val;
		parseindex = 0;
		goto top;
	}

	switch (c) {
	case '\'':
	case '"':
		endc = c;
		while (1) {
			if ((c = lgetc(fin)) == EOF)
				return (0);
			if (c == endc) {
				*p = '\0';
				break;
			}
			if (c == '\n') {
				lineno++;
				continue;
			}
			if (p + 1 >= buf + sizeof(buf) - 1) {
				yyerror("string too long");
				return (findeol());
			}
			*p++ = (char)c;
		}
		yylval.v.string = strdup(buf);
		if (yylval.v.string == NULL)
			err(1, "yylex: strdup");
		return (STRING);
	case '=':
		yylval.v.i = PF_OP_EQ;
		return (PORTUNARY);
	case '!':
		next = lgetc(fin);
		if (next == '=') {
			yylval.v.i = PF_OP_NE;
			return (PORTUNARY);
		}
		lungetc(next);
		break;
	case '<':
		next = lgetc(fin);
		if (next == '>') {
			yylval.v.i = PF_OP_XRG;
			return (PORTBINARY);
		} else if (next == '=') {
			yylval.v.i = PF_OP_LE;
		} else {
			yylval.v.i = PF_OP_LT;
			lungetc(next);
		}
		return (PORTUNARY);
		break;
	case '>':
		next = lgetc(fin);
		if (next == '<') {
			yylval.v.i = PF_OP_IRG;
			return (PORTBINARY);
		} else if (next == '=') {
			yylval.v.i = PF_OP_GE;
		} else {
			yylval.v.i = PF_OP_GT;
			lungetc(next);
		}
		return (PORTUNARY);
		break;
	case '-':
		next = lgetc(fin);
		if (next == '>')
			return (ARROW);
		lungetc(next);
		break;
	}

#define allowed_in_string(x) \
	(isalnum(x) || (ispunct(x) && x != '(' && x != ')' && \
	x != '{' && x != '}' && x != '<' && x != '>' && \
	x != '!' && x != '=' && x != '/' && x != '#' && \
	x != ','))

	if (isalnum(c) || c == ':' || c == '_') {
		do {
			*p++ = c;
			if ((unsigned)(p-buf) >= sizeof(buf)) {
				yyerror("string too long");
				return (findeol());
			}
		} while ((c = lgetc(fin)) != EOF && (allowed_in_string(c)));
		lungetc(c);
		*p = '\0';
		token = lookup(buf);
		yylval.v.string = strdup(buf);
		if (yylval.v.string == NULL)
			err(1, "yylex: strdup");
		return (token);
	}
	if (c == '\n') {
		yylval.lineno = lineno;
		lineno++;
	}
	if (c == EOF)
		return (0);
	return (c);
}

int
parse_rules(FILE *input, struct pfctl *xpf)
{
	struct sym	*sym;

	fin = input;
	pf = xpf;
	lineno = 1;
	errors = 0;
	rulestate = PFCTL_STATE_NONE;
	yyparse();

	/* Check which macros have not been used. */
	if (pf->opts & PF_OPT_VERBOSE2)
		for (sym = TAILQ_FIRST(&symhead); sym;
		    sym = TAILQ_NEXT(sym, entries))
			if (!sym->used)
				fprintf(stderr, "warning: macro '%s' not "
				    "used\n", sym->nam);
	return (errors ? -1 : 0);
}

/*
 * Over-designed efficiency is a French and German concept, so how about
 * we wait until they discover this ugliness and make it all fancy.
 */
int
symset(const char *nam, const char *val, int persist)
{
	struct sym	*sym;

	for (sym = TAILQ_FIRST(&symhead); sym && strcmp(nam, sym->nam);
	    sym = TAILQ_NEXT(sym, entries))
		;	/* nothing */

	if (sym != NULL) {
		if (sym->persist == 1)
			return (0);
		else {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entries);
			free(sym);
		}
	}
	if ((sym = calloc(1, sizeof(*sym))) == NULL)
		return (-1);

	sym->nam = strdup(nam);
	if (sym->nam == NULL) {
		free(sym);
		return (-1);
	}
	sym->val = strdup(val);
	if (sym->val == NULL) {
		free(sym->nam);
		free(sym);
		return (-1);
	}
	sym->used = 0;
	sym->persist = persist;
	TAILQ_INSERT_TAIL(&symhead, sym, entries);
	return (0);
}

int
pfctl_cmdline_symset(char *s)
{
	char	*sym, *val;
	int	 ret;

	if ((val = strrchr(s, '=')) == NULL)
		return (-1);

	if ((sym = malloc(strlen(s) - strlen(val) + 1)) == NULL)
		err(1, "pfctl_cmdline_symset: malloc");

	strlcpy(sym, s, strlen(s) - strlen(val) + 1);

	ret = symset(sym, val + 1, 1);
	free(sym);

	return (ret);
}

char *
symget(const char *nam)
{
	struct sym	*sym;

	TAILQ_FOREACH(sym, &symhead, entries)
		if (strcmp(nam, sym->nam) == 0) {
			sym->used = 1;
			return (sym->val);
		}
	return (NULL);
}

void
decide_address_family(struct node_host *n, sa_family_t *af)
{
	sa_family_t	target_af = 0;

	while (!*af && n != NULL) {
		if (n->af) {
			if (target_af == 0)
				target_af = n->af;
			if (target_af != n->af)
				return;
		}
		n = n->next;
	}
	if (!*af && target_af)
		*af = target_af;
}

void
remove_invalid_hosts(struct node_host **nh, sa_family_t *af)
{
	struct node_host	*n = *nh, *prev = NULL;

	while (n != NULL) {
		if (*af && n->af && n->af != *af) {
			/* unlink and free n */
			struct node_host *next = n->next;

			/* adjust tail pointer */
			if (n == (*nh)->tail)
				(*nh)->tail = prev;
			/* adjust previous node's next pointer */
			if (prev == NULL)
				*nh = next;
			else
				prev->next = next;
			/* free node */
			if (n->ifname != NULL)
				free(n->ifname);
			free(n);
			n = next;
		} else {
			if (n->af && !*af)
				*af = n->af;
			prev = n;
			n = n->next;
		}
	}
}

int
invalid_redirect(struct node_host *nh, sa_family_t af)
{
	if (!af) {
		yyerror("address family not given and translation "
		    "address expands to multiple address families");
		return (1);
	}
	if (nh == NULL) {
		yyerror("no translation address with matching address family "
		    "found.");
		return (1);
	}
	return (0);
}

int
atoul(char *s, u_long *ulvalp)
{
	u_long	 ulval;
	char	*ep;

	errno = 0;
	ulval = strtoul(s, &ep, 0);
	if (s[0] == '\0' || *ep != '\0')
		return (-1);
	if (errno == ERANGE && ulval == ULONG_MAX)
		return (-1);
	*ulvalp = ulval;
	return (0);
}

int
getservice(char *n)
{
	struct servent	*s;
	u_long		 ulval;

	if (atoul(n, &ulval) == 0) {
		if (ulval > 65535) {
			yyerror("illegal port value %d", ulval);
			return (-1);
		}
		return (htons(ulval));
	} else {
		s = getservbyname(n, "tcp");
		if (s == NULL)
			s = getservbyname(n, "udp");
		if (s == NULL) {
			yyerror("unknown port %s", n);
			return (-1);
		}
		return (s->s_port);
	}
}

u_int16_t
parseicmpspec(char *w, sa_family_t af)
{
	const struct icmpcodeent	*p;
	u_long				 ulval;
	u_int8_t			 icmptype;

	if (af == AF_INET)
		icmptype = returnicmpdefault >> 8;
	else
		icmptype = returnicmp6default >> 8;

	if (atoul(w, &ulval) == -1) {
		if ((p = geticmpcodebyname(icmptype, w, af)) == NULL) {
			yyerror("unknown icmp code %s", w);
			return (0);
		}
		ulval = p->code;
	}
	return (icmptype << 8 | ulval);
}
