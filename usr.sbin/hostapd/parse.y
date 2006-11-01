/*	$OpenBSD: parse.y,v 1.23 2006/06/01 22:09:09 reyk Exp $	*/

/*
 * Copyright (c) 2004, 2005, 2006 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2002 - 2005 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
 * Copyright (c) 2001 Daniel Hartmeier.  All rights reserved.
 * Copyright (c) 2001 Theo de Raadt.  All rights reserved.
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

%{
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#include <net/if_llc.h>
#include <net/bpf.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>

#include <net80211/ieee80211.h>
#include <net80211/ieee80211_radiotap.h>

#include <ctype.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "hostapd.h"

extern struct hostapd_config hostapd_cfg;
static int errors = 0;

TAILQ_HEAD(filehead, file)	 filehead = TAILQ_HEAD_INITIALIZER(filehead);
struct file {
	TAILQ_ENTRY(file)	 entry;

	char			*name;
	FILE			*stream;
	int			 lineno;
};
static struct file *file;

TAILQ_HEAD(symhead, sym)	 symhead = TAILQ_HEAD_INITIALIZER(symhead);
struct sym {
	TAILQ_ENTRY(sym)	 entry;
	int			 used;
	int			 persist;
	char			*nam;
	char			*val;
};

int		 yyerror(const char *, ...);
int		 yyparse(void);
int		 kw_cmp(const void *, const void *);
int		 lookup(char *);
int		 lgetc(void);
int		 lungetc(int);
int		 findeol(void);
int		 yylex(void);
int		 symset(const char *, const char *, int);
char		*symget(const char *);
struct file	*hostapd_add_file(struct hostapd_config *, const char *);

typedef struct {
	union {
		struct {
			u_int8_t		lladdr[IEEE80211_ADDR_LEN];
			struct hostapd_table	*table;
			u_int32_t		flags;
		} reflladdr __packed;
		struct {
			u_int16_t		alg;
			u_int16_t		transaction;
		} authalg;
		struct in_addr		in;
		char			*string;
		long			val;
		u_int16_t		reason;
		enum hostapd_op		op;
	} v;
	int lineno;
} YYSTYPE;

struct hostapd_table *table;
struct hostapd_entry *entry;
struct hostapd_frame frame, *frame_ptr;
struct hostapd_ieee80211_frame *frame_ieee80211;
u_int negative;

#define HOSTAPD_MATCH(_m)	{					\
	frame.f_flags |= negative ?				\
	    HOSTAPD_FRAME_F_##_m##_N : HOSTAPD_FRAME_F_##_m;		\
}
#define HOSTAPD_MATCH_TABLE(_m)	{					\
	frame.f_flags |= HOSTAPD_FRAME_F_##_m##_TABLE | (negative ?	\
	    HOSTAPD_FRAME_F_##_m##_N : HOSTAPD_FRAME_F_##_m);		\
}
#define HOSTAPD_MATCH_RADIOTAP(_x) {					\
	if (hostapd_cfg.c_apme_dlt == DLT_IEEE802_11 ||			\
	    (hostapd_cfg.c_apme_dlt == 0 &&				\
	    HOSTAPD_DLT == DLT_IEEE802_11)) {				\
		yyerror("option %s requires radiotap headers", #_x);	\
		YYERROR;						\
	}								\
	frame.f_radiotap |= HOSTAPD_RADIOTAP_F(RSSI);			\
	frame.f_flags |= HOSTAPD_FRAME_F_##_x;				\
}
#define HOSTAPD_IAPP_FLAG(_f) {						\
	if (negative)							\
		hostapd_cfg.c_iapp.i_flags &= ~(HOSTAPD_IAPP_F_##_f);	\
	else								\
		hostapd_cfg.c_iapp.i_flags |= (HOSTAPD_IAPP_F_##_f);	\
}

%}

%token	MODE INTERFACE IAPP HOSTAP MULTICAST BROADCAST SET SEC USEC
%token	HANDLE TYPE SUBTYPE FROM TO BSSID WITH FRAME RADIOTAP NWID PASSIVE
%token	MANAGEMENT DATA PROBE BEACON ATIM ANY DS NO DIR RESEND RANDOM
%token	AUTH DEAUTH ASSOC DISASSOC REASSOC REQUEST RESPONSE PCAP RATE
%token	ERROR CONST TABLE NODE DELETE ADD LOG VERBOSE LIMIT QUICK SKIP
%token	REASON UNSPECIFIED EXPIRE LEAVE ASSOC TOOMANY NOT AUTHED ASSOCED
%token	RESERVED RSN REQUIRED INCONSISTENT IE INVALID MIC FAILURE OPEN
%token	ADDRESS PORT ON NOTIFY TTL INCLUDE ROUTE ROAMING RSSI TXRATE FREQ
%token	<v.string>	STRING
%token	<v.val>		VALUE
%type	<v.val>		number
%type	<v.in>		ipv4addr
%type	<v.reflladdr>	refaddr, lladdr, randaddr, frmactionaddr, frmmatchaddr
%type	<v.reason>	frmreason_l
%type	<v.string>	table
%type	<v.string>	string
%type	<v.authalg>	authalg
%type	<v.op>		unaryop
%type	<v.val>		percent
%type	<v.val>		txrate
%type	<v.val>		freq

%%

/*
 * Configuration grammar
 */

grammar		: /* empty */
		| grammar '\n'
		| grammar include '\n'
		| grammar tabledef '\n'
		| grammar option '\n'
		| grammar event '\n'
		| grammar varset '\n'
		| grammar error '\n'		{ errors++; }
		;

include		: INCLUDE STRING
		{
			struct file *nfile;

			if ((nfile =
			    hostapd_add_file(&hostapd_cfg, $2)) == NULL) {
				yyerror("failed to include file %s", $2);
				free($2);
				YYERROR;
			}
			free($2);

			file = nfile;
			lungetc('\n');
		}

option		: SET HOSTAP INTERFACE hostapifaces
		{
			if (!TAILQ_EMPTY(&hostapd_cfg.c_apmes))
				hostapd_cfg.c_flags |= HOSTAPD_CFG_F_APME;
		}
		| SET HOSTAP MODE hostapmode
		| SET IAPP INTERFACE STRING passive
		{
			strlcpy(hostapd_cfg.c_iapp.i_iface, $4,
			    sizeof(hostapd_cfg.c_iapp.i_iface));

			hostapd_cfg.c_flags |= HOSTAPD_CFG_F_IAPP;

			hostapd_log(HOSTAPD_LOG_DEBUG,
			    "%s: IAPP interface added", $4);

			free($4);
		}
		| SET IAPP MODE iappmode
		| SET IAPP ADDRESS ROAMING TABLE table
		{
			if ((hostapd_cfg.c_iapp.i_addr_tbl =
			    hostapd_table_lookup(&hostapd_cfg, $6)) == NULL) {
				yyerror("undefined table <%s>", $6);
				free($6);
				YYERROR;
			}
			free($6);
		}
		| SET IAPP ROUTE ROAMING TABLE table
		{
			if ((hostapd_cfg.c_iapp.i_route_tbl =
			    hostapd_table_lookup(&hostapd_cfg, $6)) == NULL) {
				yyerror("undefined table <%s>", $6);
				free($6);
				YYERROR;
			}
			free($6);
		}
		| SET IAPP HANDLE SUBTYPE iappsubtypes
		;

iappmode	: MULTICAST iappmodeaddr iappmodeport iappmodettl
		{
			hostapd_cfg.c_flags &= ~HOSTAPD_CFG_F_BRDCAST;
		}
		| BROADCAST iappmodeport
		{
			hostapd_cfg.c_flags |= HOSTAPD_CFG_F_BRDCAST;
		}
		;

iappmodeaddr	: /* empty */
		| ADDRESS ipv4addr
		{
			bcopy(&$2, &hostapd_cfg.c_iapp.i_multicast.sin_addr,
			    sizeof(struct in_addr));
		}
		;

iappmodeport	: /* empty */
		| PORT number
		{
			hostapd_cfg.c_iapp.i_addr.sin_port = htons($2);
		}
		;

iappmodettl	: /* empty */
		| TTL number
		{
			hostapd_cfg.c_iapp.i_ttl = $2;
		}
		;

hostapmode	: RADIOTAP
		{
			hostapd_cfg.c_apme_dlt = DLT_IEEE802_11_RADIO;
		}
		| PCAP
		{
			hostapd_cfg.c_apme_dlt = DLT_IEEE802_11;
		}
		;

hostapifaces	: '{' optnl hostapifacelist optnl '}'
		| hostapiface
		;

hostapifacelist	: hostapiface
		| hostapifacelist comma hostapiface
		;

hostapiface	: STRING
		{
			if (hostapd_apme_add(&hostapd_cfg, $1) != 0) {
				yyerror("failed to add hostap interface");
				YYERROR;
			}
			free($1);
		}
		;

hostapmatch	: /* empty */
		| ON STRING
		{
			if ((frame.f_apme =
			    hostapd_apme_lookup(&hostapd_cfg, $2)) == NULL) {
				yyerror("undefined hostap interface");
				free($2);
				YYERROR;
			}
			free($2);

			HOSTAPD_MATCH(APME);
		}
		;

event		: HOSTAP HANDLE
		{
			bzero(&frame, sizeof(struct hostapd_frame));
			/* IEEE 802.11 frame to match */
			frame_ieee80211 = &frame.f_frame;
		} eventopt hostapmatch frmmatch {
			/* IEEE 802.11 raw frame to send as an action */
			frame_ieee80211 = &frame.f_action_data.a_frame;
		} action limit rate {
			if ((frame_ptr = (struct hostapd_frame *)calloc(1,
				 sizeof(struct hostapd_frame))) == NULL) {
				yyerror("calloc");
				YYERROR;
			}

			gettimeofday(&frame.f_last, NULL);
			timeradd(&frame.f_last, &frame.f_limit, &frame.f_then);

			bcopy(&frame, frame_ptr, sizeof(struct hostapd_frame));
			TAILQ_INSERT_TAIL(&hostapd_cfg.c_frames,
			    frame_ptr, f_entries);
		}
		;

iappsubtypes	: '{' optnl iappsubtypelist optnl '}'
		| iappsubtype
		;

iappsubtypelist	: iappsubtype
		| iappsubtypelist comma iappsubtype
		;

iappsubtype	: not ADD NOTIFY
		{
			HOSTAPD_IAPP_FLAG(ADD_NOTIFY);
		}
		| not RADIOTAP
		{
			HOSTAPD_IAPP_FLAG(RADIOTAP);
		}
		| not ROUTE ROAMING
		{
			HOSTAPD_IAPP_FLAG(ROAMING_ROUTE);
		}
		| not ADDRESS ROAMING
		{
			HOSTAPD_IAPP_FLAG(ROAMING_ADDRESS);
		}
		;

eventopt	: /* empty */
		{
			frame.f_flags |= HOSTAPD_FRAME_F_RET_OK;
		}
		| QUICK
		{
			frame.f_flags |= HOSTAPD_FRAME_F_RET_QUICK;
		}
		| SKIP
		{
			frame.f_flags |= HOSTAPD_FRAME_F_RET_SKIP;
		}
		;

action		: /* empty */
		{
			frame.f_action = HOSTAPD_ACTION_NONE;
		}
		| WITH LOG verbose
		{
			frame.f_action = HOSTAPD_ACTION_LOG;
		}
		| WITH FRAME frmaction
		{
			frame.f_action = HOSTAPD_ACTION_FRAME;
		}
		| WITH IAPP iapp
		| WITH NODE nodeopt frmactionaddr
		{
			if (($4.flags & HOSTAPD_ACTION_F_REF_M) == 0) {
				bcopy($4.lladdr, frame.f_action_data.a_lladdr,
				    IEEE80211_ADDR_LEN);
			} else
				frame.f_action_data.a_flags |= $4.flags;
		}
		| WITH RESEND
		{
			frame.f_action = HOSTAPD_ACTION_RESEND;
		}
		;

verbose		: /* empty */
		| VERBOSE
		{
			frame.f_action_flags |= HOSTAPD_ACTION_VERBOSE;
		}
		;

iapp		: TYPE RADIOTAP verbose
		{
			frame.f_action = HOSTAPD_ACTION_RADIOTAP;
		}
		;

nodeopt		: DELETE
		{
			frame.f_action = HOSTAPD_ACTION_DELNODE;
		}
		| ADD
		{
			frame.f_action = HOSTAPD_ACTION_ADDNODE;
		}
		;

frmmatch	: ANY
		| frm frmmatchtype frmmatchdir frmmatchfrom frmmatchto
			frmmatchbssid frmmatchrtap
		;

frm		: /* empty */
		| FRAME
		;

frmaction	: frmactiontype frmactiondir frmactionfrom frmactionto frmactionbssid
		;

limit		: /* empty */
		| LIMIT number SEC
		{
			frame.f_limit.tv_sec = $2;
		}
		| LIMIT number USEC
		{
			frame.f_limit.tv_usec = $2;
		}
		;

rate		: /* empty */
		| RATE number '/' number SEC
		{
			if (!($2 && $4)) {
				yyerror("invalid rate");
				YYERROR;
			}

			frame.f_rate = $2;
			frame.f_rate_intval = $4;
		}
		;

frmmatchtype	: /* any */
		| TYPE ANY
		| TYPE not DATA
		{
			frame_ieee80211->i_fc[0] |=
			    IEEE80211_FC0_TYPE_DATA;
			HOSTAPD_MATCH(TYPE);
		}
		| TYPE not MANAGEMENT frmmatchmgmt
		{
			frame_ieee80211->i_fc[0] |=
			    IEEE80211_FC0_TYPE_MGT;
			HOSTAPD_MATCH(TYPE);
		}
		;

frmmatchmgmt	: /* any */
		| SUBTYPE ANY
		| SUBTYPE not frmsubtype
		{
			HOSTAPD_MATCH(SUBTYPE);
		}
		;

frmsubtype	: PROBE REQUEST frmelems
		{
			frame_ieee80211->i_fc[0] |=
			    IEEE80211_FC0_SUBTYPE_PROBE_REQ;
		}
		| PROBE RESPONSE frmelems
		{
			frame_ieee80211->i_fc[0] |=
			    IEEE80211_FC0_SUBTYPE_PROBE_RESP;
		}
		| BEACON frmelems
		{
			frame_ieee80211->i_fc[0] |=
			    IEEE80211_FC0_SUBTYPE_BEACON;
		}
		| ATIM
		{
			frame_ieee80211->i_fc[0] |=
			    IEEE80211_FC0_SUBTYPE_ATIM;
		}
		| AUTH frmauth
		{
			frame_ieee80211->i_fc[0] |=
			    IEEE80211_FC0_SUBTYPE_AUTH;
		}
		| DEAUTH frmreason
		{
			frame_ieee80211->i_fc[0] |=
			    IEEE80211_FC0_SUBTYPE_DEAUTH;
		}
		| ASSOC REQUEST
		{
			frame_ieee80211->i_fc[0] |=
			    IEEE80211_FC0_SUBTYPE_ASSOC_REQ;
		}
		| DISASSOC frmreason
		{
			frame_ieee80211->i_fc[0] |=
			    IEEE80211_FC0_SUBTYPE_DISASSOC;
		}
		| ASSOC RESPONSE
		{
			frame_ieee80211->i_fc[0] |=
			    IEEE80211_FC0_SUBTYPE_ASSOC_RESP;
		}
		| REASSOC REQUEST
		{
			frame_ieee80211->i_fc[0] |=
			    IEEE80211_FC0_SUBTYPE_REASSOC_REQ;
		}
		| REASSOC RESPONSE
		{
			frame_ieee80211->i_fc[0] |=
			    IEEE80211_FC0_SUBTYPE_REASSOC_RESP;
		}
		;

frmelems	: /* empty */
		| frmelems_l
		;

frmelems_l	: frmelems_l frmelem
		| frmelem
		;

frmelem		: NWID not STRING
		;

frmauth		: /* empty */
		| authalg
		{
			if ((frame_ieee80211->i_data = malloc(6)) == NULL) {
				yyerror("failed to allocate auth");
				YYERROR;
			}
			((u_int16_t *)frame_ieee80211->i_data)[0] =
				$1.alg;
			((u_int16_t *)frame_ieee80211->i_data)[1] =
				$1.transaction;
			((u_int16_t *)frame_ieee80211->i_data)[0] = 0;
			frame_ieee80211->i_data_len = 6;
		}
		;

authalg		: OPEN REQUEST
		{
			$$.alg = htole16(IEEE80211_AUTH_ALG_OPEN);
			$$.transaction = htole16(IEEE80211_AUTH_OPEN_REQUEST);
		}
		| OPEN RESPONSE
		{
			$$.alg = htole16(IEEE80211_AUTH_ALG_OPEN);
			$$.transaction = htole16(IEEE80211_AUTH_OPEN_RESPONSE);
		}
		;

frmreason	: frmreason_l
		{
			if ($1 != 0) {
				if ((frame_ieee80211->i_data = (u_int16_t *)
				    malloc(sizeof(u_int16_t))) == NULL) {
					yyerror("failed to allocate "
					    "reason code %u", $1);
					YYERROR;
				}
				*(u_int16_t *)frame_ieee80211->i_data =
				    htole16($1);
				frame_ieee80211->i_data_len = sizeof(u_int16_t);
			}
		}
		;

frmreason_l	: /* empty */
		{
			$$ = 0;
		}
		| REASON UNSPECIFIED
		{
			$$ = IEEE80211_REASON_UNSPECIFIED;
		}
		| REASON AUTH EXPIRE
		{
			$$ = IEEE80211_REASON_AUTH_EXPIRE;
		}
		| REASON AUTH LEAVE
		{
			$$ = IEEE80211_REASON_AUTH_LEAVE;
		}
		| REASON ASSOC EXPIRE
		{
			$$ = IEEE80211_REASON_ASSOC_EXPIRE;
		}
		| REASON ASSOC TOOMANY
		{
			$$ = IEEE80211_REASON_ASSOC_TOOMANY;
		}
		| REASON NOT AUTHED
		{
			$$ = IEEE80211_REASON_NOT_AUTHED;
		}
		| REASON NOT ASSOCED
		{
			$$ = IEEE80211_REASON_NOT_ASSOCED;
		}
		| REASON ASSOC LEAVE
		{
			$$ = IEEE80211_REASON_ASSOC_LEAVE;
		}
		| REASON ASSOC NOT AUTHED
		{
			$$ = IEEE80211_REASON_NOT_AUTHED;
		}
		| REASON RESERVED
		{
			$$ = 10;	/* XXX unknown */
		}
		| REASON RSN REQUIRED
		{
			$$ = IEEE80211_REASON_RSN_REQUIRED;
		}
		| REASON RSN INCONSISTENT
		{
			$$ = IEEE80211_REASON_RSN_INCONSISTENT;
		}
		| REASON IE INVALID
		{
			$$ = IEEE80211_REASON_IE_INVALID;
		}
		| REASON MIC FAILURE
		{
			$$ = IEEE80211_REASON_MIC_FAILURE;
		}
		;

frmmatchdir	: /* any */
		| DIR ANY
		| DIR frmdir
		{
			HOSTAPD_MATCH(DIR);
		}
		;

frmdir		: NO DS
		{
			frame_ieee80211->i_fc[1] |= IEEE80211_FC1_DIR_NODS;
		}
		| TO DS
		{
			frame_ieee80211->i_fc[1] |= IEEE80211_FC1_DIR_TODS;
		}
		| FROM DS
		{
			frame_ieee80211->i_fc[1] |= IEEE80211_FC1_DIR_FROMDS;
		}
		| DS TO DS
		{
			frame_ieee80211->i_fc[1] |= IEEE80211_FC1_DIR_DSTODS;
		}
		;

frmmatchfrom	: /* any */
		| FROM frmmatchaddr
		{
			if (($2.flags & HOSTAPD_ACTION_F_OPT_TABLE) == 0) {
				bcopy($2.lladdr, &frame_ieee80211->i_from,
				    IEEE80211_ADDR_LEN);
				HOSTAPD_MATCH(FROM);
			} else {
				frame.f_from = $2.table;
				HOSTAPD_MATCH_TABLE(FROM);
			}
		}
		;

frmmatchto	: /* any */
		| TO frmmatchaddr
		{
			if (($2.flags & HOSTAPD_ACTION_F_OPT_TABLE) == 0) {
				bcopy($2.lladdr, &frame_ieee80211->i_to,
				    IEEE80211_ADDR_LEN);
				HOSTAPD_MATCH(TO);
			} else {
				frame.f_to = $2.table;
				HOSTAPD_MATCH_TABLE(TO);
			}
		}
		;

frmmatchbssid	: /* any */
		| BSSID frmmatchaddr
		{
			if (($2.flags & HOSTAPD_ACTION_F_OPT_TABLE) == 0) {
				bcopy($2.lladdr, &frame_ieee80211->i_bssid,
				    IEEE80211_ADDR_LEN);
				HOSTAPD_MATCH(BSSID);
			} else {
				frame.f_bssid = $2.table;
				HOSTAPD_MATCH_TABLE(BSSID);
			}
		}
		;

frmmatchrtap	: /* empty */
		| frmmatchrtap_l
		;

frmmatchrtap_l	: frmmatchrtap_l frmmatchrtapopt
		| frmmatchrtapopt
		;

frmmatchrtapopt	: RSSI unaryop percent
		{
			if (($2 == HOSTAPD_OP_GT && $3 == 100) ||
			    ($2 == HOSTAPD_OP_LE && $3 == 100) ||
			    ($2 == HOSTAPD_OP_LT && $3 == 0) ||
			    ($2 == HOSTAPD_OP_GE && $3 == 0)) {
				yyerror("absurd unary comparison");
				YYERROR;
			}

			frame.f_rssi_op = $2;
			frame.f_rssi = $3;
			HOSTAPD_MATCH_RADIOTAP(RSSI);
		}
		| TXRATE unaryop txrate
		{
			frame.f_txrate_op = $2;
			frame.f_txrate = $3;
			HOSTAPD_MATCH_RADIOTAP(RATE);
		}
		| FREQ unaryop freq
		{
			frame.f_chan_op = $2;
			frame.f_chan = $3;
			HOSTAPD_MATCH_RADIOTAP(CHANNEL);
		}
		;

frmmatchaddr	: ANY
		{
			$$.flags = 0;
		}
		| not table
		{
			if (($$.table =
			    hostapd_table_lookup(&hostapd_cfg, $2)) == NULL) {
				yyerror("undefined table <%s>", $2);
				free($2);
				YYERROR;
			}
			$$.flags = HOSTAPD_ACTION_F_OPT_TABLE;
			free($2);
		}
		| not lladdr
		{
			bcopy($2.lladdr, $$.lladdr, IEEE80211_ADDR_LEN);
			$$.flags = HOSTAPD_ACTION_F_OPT_TABLE;
		}
		;

frmactiontype	: TYPE DATA
		{
			frame_ieee80211->i_fc[0] |= IEEE80211_FC0_TYPE_DATA;
		}
		| TYPE MANAGEMENT frmactionmgmt
		{
			frame_ieee80211->i_fc[0] |= IEEE80211_FC0_TYPE_MGT;
		}
		;

frmactionmgmt	: SUBTYPE frmsubtype
		;

frmactiondir	: /* empty */
		{
			frame.f_action_data.a_flags |=
			    HOSTAPD_ACTION_F_OPT_DIR_AUTO;
		}
		| DIR frmdir
		;

frmactionfrom	: FROM frmactionaddr
		{
			if (($2.flags & HOSTAPD_ACTION_F_REF_M) == 0) {
				bcopy($2.lladdr, frame_ieee80211->i_from,
				    IEEE80211_ADDR_LEN);
			} else
				frame.f_action_data.a_flags |=
				    ($2.flags << HOSTAPD_ACTION_F_REF_FROM_S);
		}
		;

frmactionto	: TO frmactionaddr
		{
			if (($2.flags & HOSTAPD_ACTION_F_REF_M) == 0) {
				bcopy($2.lladdr, frame_ieee80211->i_to,
				    IEEE80211_ADDR_LEN);
			} else
				frame.f_action_data.a_flags |=
				    ($2.flags << HOSTAPD_ACTION_F_REF_TO_S);
		}
		;

frmactionbssid	: BSSID frmactionaddr
		{
			if (($2.flags & HOSTAPD_ACTION_F_REF_M) == 0) {
				bcopy($2.lladdr, frame_ieee80211->i_bssid,
				    IEEE80211_ADDR_LEN);
			} else
				frame.f_action_data.a_flags |=
				    ($2.flags << HOSTAPD_ACTION_F_REF_BSSID_S);
		}
		;

frmactionaddr	: lladdr
		{
			bcopy($1.lladdr, $$.lladdr, IEEE80211_ADDR_LEN);
			$$.flags = $1.flags;
		}
		| randaddr
		{
			$$.flags = $1.flags;
		}
		| refaddr
		{
			$$.flags = $1.flags;
		}
		;

table		: '<' STRING '>' {
			if (strlen($2) >= HOSTAPD_TABLE_NAMELEN) {
				yyerror("table name %s too long, max %u",
				    $2, HOSTAPD_TABLE_NAMELEN - 1);
				free($2);
				YYERROR;
			}
			$$ = $2;
		}
		;

tabledef	: TABLE table {
			if ((table =
			    hostapd_table_add(&hostapd_cfg, $2)) == NULL) {
				yyerror("failed to add table: %s", $2);
				free($2);
				YYERROR;
			}
			free($2);
		} tableopts {
			table = NULL;
		}
		;

tableopts	: /* empty */
		| tableopts_l
		;

tableopts_l	: tableopts_l tableopt
		| tableopt
		;

tableopt	: CONST	{
			if (table->t_flags & HOSTAPD_TABLE_F_CONST) {
				yyerror("option already specified");
				YYERROR;
			}
			table->t_flags |= HOSTAPD_TABLE_F_CONST;
		}
		| '{' optnl '}'
		| '{' optnl tableaddrlist optnl '}'
		;

string		: string STRING
		{
			if (asprintf(&$$, "%s %s", $1, $2) == -1)
				hostapd_fatal("string: asprintf");
			free($1);
			free($2);
		}
		| STRING
		;

varset		: STRING '=' string
		{
			if (symset($1, $3, 0) == -1)
				hostapd_fatal("cannot store variable");
			free($1);
			free($3);
		}
		;

refaddr		: '&' FROM
		{
			$$.flags |= HOSTAPD_ACTION_F_REF_FROM;
		}
		| '&' TO
		{
			$$.flags |= HOSTAPD_ACTION_F_REF_TO;
		}
		| '&' BSSID
		{
			$$.flags |= HOSTAPD_ACTION_F_REF_BSSID;
		}
		;

tableaddrlist	: tableaddrentry
		| tableaddrlist comma tableaddrentry
		;

tableaddrentry	: lladdr
		{
			if ((entry = hostapd_entry_add(table,
			    $1.lladdr)) == NULL) {
				yyerror("failed to add entry: %s",
				    etheraddr_string($1.lladdr));
				YYERROR;
			}
		} tableaddropt {
			entry = NULL;
		}
		;

tableaddropt	: /* empty */
		| assign ipv4addr ipnetmask
		{
			entry->e_flags |= HOSTAPD_ENTRY_F_INADDR;
			entry->e_inaddr.in_af = AF_INET;
			bcopy(&$2, &entry->e_inaddr.in_v4,
			    sizeof(struct in_addr));
		}
		| mask lladdr
		{
			entry->e_flags |= HOSTAPD_ENTRY_F_MASK;
			bcopy($2.lladdr, entry->e_mask, IEEE80211_ADDR_LEN);

			/* Update entry position in the table */
			hostapd_entry_update(table, entry);
		}
		;

ipv4addr	: STRING
		{
			if (inet_net_pton(AF_INET, $1, &$$, sizeof($$)) == -1) {
				yyerror("invalid address: %s\n", $1);
				free($1);
				YYERROR;
			}
			free($1);
		}
		;

ipnetmask	: /* empty */
		{
			entry->e_inaddr.in_netmask = -1;
		}
		| '/' number
		{
			entry->e_inaddr.in_netmask = $2;
		}
		;

lladdr		: STRING
		{
			struct ether_addr *ea;

			if ((ea = ether_aton($1)) == NULL) {
				yyerror("invalid address: %s\n", $1);
				free($1);
				YYERROR;
			}
			free($1);

			bcopy(ea, $$.lladdr, IEEE80211_ADDR_LEN);
			$$.flags = HOSTAPD_ACTION_F_OPT_LLADDR;
		}
		;

randaddr	: RANDOM
		{
			$$.flags |= HOSTAPD_ACTION_F_REF_RANDOM;
		}
		;

number		: STRING
		{
			$$ = strtonum($1, 0, LONG_MAX, NULL);
			free($1);
		}
		;

passive		: /* empty */
		| PASSIVE
		{
			hostapd_cfg.c_flags |= HOSTAPD_CFG_F_IAPP_PASSIVE;
		}
		;

assign		: '-' '>'
		;

mask		: '&'
		;

comma		: /* emtpy */
		| ',' optnl
		;

optnl		: /* empty */
		| '\n'
		;

not		: /* empty */
		{
			negative = 0;
		}
		| '!'
		{
			negative = 1;
		}
		| NOT
		{
			negative = 1;
		}
		;

unaryop		: /* any */
		{
			$$ = HOSTAPD_OP_EQ;
		}
		| '='
		{
			$$ = HOSTAPD_OP_EQ;
		}
		| '=='
		{
			$$ = HOSTAPD_OP_EQ;
		}
		| '!'
		{
			$$ = HOSTAPD_OP_NE;
		}
		| '!' '='
		{
			$$ = HOSTAPD_OP_NE;
		}
		| '<' '='
		{
			$$ = HOSTAPD_OP_LE;
		}
		| '<'
		{
			$$ = HOSTAPD_OP_LT;
		}
		| '>' '='
		{
			$$ = HOSTAPD_OP_GE;
		}
		| '>'
		{
			$$ = HOSTAPD_OP_GT;
		}
		;

percent		: STRING
		{
			double val;
			char *cp;

			val = strtod($1, &cp);
			if (cp == NULL || strcmp(cp, "%") != 0 ||
			    val < 0 || val > 100) {
				yyerror("invalid percentage: %s", $1);
				free($1);
				YYERROR;
			}
			free($1);
			$$ = val;
		}
		;

txrate		: STRING
		{
			double val;
			char *cp;

			val = strtod($1, &cp) * 2;
			if (cp == NULL || strcasecmp(cp, "mb") != 0 ||
			    val != (int)val) {
				yyerror("invalid rate: %s", $1);
				free($1);
				YYERROR;
			}
			free($1);
			$$ = val;
		}
		;

freq		: STRING
		{
			double val;
			char *cp;

			val = strtod($1, &cp);
			if (cp != NULL) {
				if (strcasecmp(cp, "ghz") == 0) {
					$$ = val * 1000;
				} else if (strcasecmp(cp, "mhz") == 0) {
					$$ = val;
				} else
					cp = NULL;
			}
			if (cp == NULL) {
				yyerror("invalid frequency: %s", $1);
				free($1);
				YYERROR;
			}
			free($1);
		}
		;
%%

/*
 * Parser and lexer
 */

struct keywords {
	char *k_name;
	int k_val;
};

int
kw_cmp(const void *a, const void *b)
{
	return strcmp(a, ((const struct keywords *)b)->k_name);
}

int
lookup(char *token)
{
	/* Keep this list sorted */
	static const struct keywords keywords[] = {
		{ "add",		ADD },
		{ "address",		ADDRESS },
		{ "any",		ANY },
		{ "assoc",		ASSOC },
		{ "assoced",		ASSOCED },
		{ "atim",		ATIM },
		{ "auth",		AUTH },
		{ "authed",		AUTHED },
		{ "beacon",		BEACON },
		{ "broadcast",		BROADCAST },
		{ "bssid",		BSSID },
		{ "const",		CONST },
		{ "data",		DATA },
		{ "deauth",		DEAUTH },
		{ "delete",		DELETE },
		{ "dir",		DIR },
		{ "disassoc",		DISASSOC },
		{ "ds",			DS },
		{ "expire",		EXPIRE },
		{ "failure",		FAILURE },
		{ "frame",		FRAME },
		{ "freq",		FREQ },
		{ "from",		FROM },
		{ "handle",		HANDLE },
		{ "hostap",		HOSTAP },
		{ "iapp",		IAPP },
		{ "ie",			IE },
		{ "include",		INCLUDE },
		{ "inconsistent",	INCONSISTENT },
		{ "interface",		INTERFACE },
		{ "invalid",		INVALID },
		{ "leave",		LEAVE },
		{ "limit",		LIMIT },
		{ "log",		LOG },
		{ "management",		MANAGEMENT },
		{ "mic",		MIC },
		{ "mode",		MODE },
		{ "multicast",		MULTICAST },
		{ "no",			NO },
		{ "node",		NODE },
		{ "not",		NOT },
		{ "notify",		NOTIFY },
		{ "nwid",		NWID },
		{ "on",			ON },
		{ "open",		OPEN },
		{ "passive",		PASSIVE },
		{ "pcap",		PCAP },
		{ "port",		PORT },
		{ "probe",		PROBE },
		{ "quick",		QUICK },
		{ "radiotap",		RADIOTAP },
		{ "random",		RANDOM },
		{ "rate",		RATE },
		{ "reason",		REASON },
		{ "reassoc",		REASSOC },
		{ "request",		REQUEST },
		{ "required",		REQUIRED },
		{ "resend",		RESEND },
		{ "reserved",		RESERVED },
		{ "response",		RESPONSE },
		{ "roaming",		ROAMING },
		{ "route",		ROUTE },
		{ "rsn",		RSN },
		{ "sec",		SEC },
		{ "set",		SET },
		{ "signal",		RSSI },
		{ "skip",		SKIP },
		{ "subtype",		SUBTYPE },
		{ "table",		TABLE },
		{ "to",			TO },
		{ "toomany",		TOOMANY },
		{ "ttl",		TTL },
		{ "txrate",		TXRATE },
		{ "type",		TYPE },
		{ "unspecified",	UNSPECIFIED },
		{ "usec",		USEC },
		{ "verbose",		VERBOSE },
		{ "with",		WITH }
	};
	const struct keywords *p;

	p = bsearch(token, keywords, sizeof(keywords) / sizeof(keywords[0]),
	    sizeof(keywords[0]), kw_cmp);

	return (p == NULL ? STRING : p->k_val);
}

#define MAXPUSHBACK	128

char	*parsebuf;
int	 parseindex;
char	 pushback_buffer[MAXPUSHBACK];
int	 pushback_index = 0;

int
lgetc(void)
{
	int	c, next;
	struct file *pfile;

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

	while ((c = getc(file->stream)) == '\\') {
		next = getc(file->stream);
		if (next != '\n') {
			c = next;
			break;
		}
		yylval.lineno = file->lineno;
		file->lineno++;
	}
	if (c == '\t' || c == ' ') {
		/* Compress blanks to a single space. */
		do {
			c = getc(file->stream);
		} while (c == '\t' || c == ' ');
		ungetc(c, file->stream);
		c = ' ';
	}

	while (c == EOF &&
	    (pfile = TAILQ_PREV(file, filehead, entry)) != NULL) {
		fclose(file->stream);
		free(file->name);
		TAILQ_REMOVE(&filehead, file, entry);
		free(file);

		file = pfile;
		c = getc(file->stream);
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
		c = lgetc();
		if (c == '\n') {
			file->lineno++;
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
	int	 endc, c;
	int	 token;

top:
	p = buf;
	while ((c = lgetc()) == ' ')
		; /* nothing */

	yylval.lineno = file->lineno;
	if (c == '#')
		while ((c = lgetc()) != '\n' && c != EOF)
			; /* nothing */
	if (c == '$' && parsebuf == NULL) {
		while (1) {
			if ((c = lgetc()) == EOF)
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
			yyerror("macro \"%s\" not defined", buf);
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
			if ((c = lgetc()) == EOF)
				return (0);
			if (c == endc) {
				*p = '\0';
				break;
			}
			if (c == '\n') {
				file->lineno++;
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
			hostapd_fatal("yylex: strdup");
		return (STRING);
	}

#define allowed_in_string(x) \
	(isalnum(x) || (ispunct(x) && x != '(' && x != ')' && \
	x != '{' && x != '}' && x != '<' && x != '>' && \
	x != '!' && x != '=' && x != '/' && x != '#' && \
	x != ','))

	if (isalnum(c) || c == ':' || c == '_' || c == '*') {
		do {
			*p++ = c;
			if ((unsigned)(p-buf) >= sizeof(buf)) {
				yyerror("string too long");
				return (findeol());
			}
		} while ((c = lgetc()) != EOF && (allowed_in_string(c)));
		lungetc(c);
		*p = '\0';
		if ((token = lookup(buf)) == STRING)
			if ((yylval.v.string = strdup(buf)) == NULL)
				hostapd_fatal("yylex: strdup");
		return (token);
	}
	if (c == '\n') {
		yylval.lineno = file->lineno;
		file->lineno++;
	}
	if (c == EOF)
		return (0);
	return (c);
}

int
symset(const char *nam, const char *val, int persist)
{
	struct sym	*sym;

	for (sym = TAILQ_FIRST(&symhead); sym && strcmp(nam, sym->nam);
	    sym = TAILQ_NEXT(sym, entry))
		;	/* nothing */

	if (sym != NULL) {
		if (sym->persist == 1)
			return (0);
		else {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entry);
			free(sym);
		}
	}
	if ((sym = (struct sym *)calloc(1, sizeof(*sym))) == NULL)
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
	TAILQ_INSERT_TAIL(&symhead, sym, entry);

	hostapd_log(HOSTAPD_LOG_DEBUG, "%s = \"%s\"", sym->nam, sym->val);

	return (0);
}

int
hostapd_parse_symset(char *s)
{
	char	*sym, *val;
	int	ret;
	size_t	len;

	if ((val = strrchr(s, '=')) == NULL)
		return (-1);

	len = strlen(s) - strlen(val) + 1;
	if ((sym = (char *)malloc(len)) == NULL)
		hostapd_fatal("cmdline_symset: malloc");

	strlcpy(sym, s, len);

	ret = symset(sym, val + 1, 1);

	free(sym);

	return (ret);
}

char *
symget(const char *nam)
{
	struct sym	*sym;

	TAILQ_FOREACH(sym, &symhead, entry)
		if (strcmp(nam, sym->nam) == 0) {
			sym->used = 1;
			return (sym->val);
		}
	return (NULL);
}

struct file *
hostapd_add_file(struct hostapd_config *cfg, const char *name)
{
	struct file *nfile = NULL;

	if ((nfile = calloc(1, sizeof(struct file))) == NULL)
		return (NULL);

	if ((nfile->name = strdup(name)) == NULL)
		goto err;

	if ((nfile->stream = fopen(name, "rb")) == NULL) {
		hostapd_log(HOSTAPD_LOG, "failed to open %s", name);
		goto err;
	}

	if (hostapd_check_file_secrecy(fileno(nfile->stream), name)) {
		hostapd_log(HOSTAPD_LOG, "invalid permissions for %s", name);
		goto err;
	}

	nfile->lineno = 1;
	TAILQ_INSERT_TAIL(&filehead, nfile, entry);

	return (nfile);

 err:
	if (nfile->name != NULL)
		free(nfile->name);
	if (nfile->stream != NULL)
		fclose(nfile->stream);
	free(nfile);

	return (NULL);
}

int
hostapd_parse_file(struct hostapd_config *cfg)
{
	struct sym *sym, *next;
	struct file *nfile;
	int ret;

	if ((file = hostapd_add_file(cfg, cfg->c_config)) == NULL)
		hostapd_fatal("failed to open the main config file: %s\n",
		    cfg->c_config);

	/* Init tables and data structures */
	TAILQ_INIT(&cfg->c_apmes);
	TAILQ_INIT(&cfg->c_tables);
	TAILQ_INIT(&cfg->c_frames);
	cfg->c_iapp.i_multicast.sin_addr.s_addr = INADDR_ANY;
	cfg->c_iapp.i_flags = HOSTAPD_IAPP_F_DEFAULT;
	cfg->c_iapp.i_ttl = IP_DEFAULT_MULTICAST_TTL;

	errors = 0;

	ret = yyparse();

	for (file = TAILQ_FIRST(&filehead); file != NULL; file = nfile) {
		nfile = TAILQ_NEXT(file, entry);
		fclose(file->stream);
		free(file->name);
		TAILQ_REMOVE(&filehead, file, entry);
		free(file);
	}

	/* Free macros and check which have not been used. */
	for (sym = TAILQ_FIRST(&symhead); sym != NULL; sym = next) {
		next = TAILQ_NEXT(sym, entry);
		if (!sym->used)
			hostapd_log(HOSTAPD_LOG_VERBOSE,
			    "warning: macro \"%s\" not used", sym->nam);
		if (!sym->persist) {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entry);
			free(sym);
		}
	}

	return (errors ? EINVAL : ret);
}

int
yyerror(const char *fmt, ...)
{
	va_list ap;
	char *nfmt;

	errors = 1;

	va_start(ap, fmt);
	if (asprintf(&nfmt, "%s:%d: %s\n", file->name, yylval.lineno,
	    fmt) == -1)
		hostapd_fatal("yyerror asprintf");
	vfprintf(stderr, nfmt, ap);
	fflush(stderr);
	va_end(ap);
	free(nfmt);

	return (0);
}
