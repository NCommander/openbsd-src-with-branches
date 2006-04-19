/*	$OpenBSD: parse.y,v 1.60 2006/04/19 16:10:50 hshoexer Exp $	*/

/*
 * Copyright (c) 2002, 2003, 2004 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2001 Markus Friedl.  All rights reserved.
 * Copyright (c) 2001 Daniel Hartmeier.  All rights reserved.
 * Copyright (c) 2001 Theo de Raadt.  All rights reserved.
 * Copyright (c) 2004, 2005 Hans-Joerg Hoexer <hshoexer@openbsd.org>
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
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip_ipsp.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <limits.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <netdb.h>

#include "ipsecctl.h"

#define KEYSIZE_LIMIT	1024

static struct ipsecctl	*ipsec = NULL;
static FILE		*fin = NULL;
static int		 lineno = 1;
static int		 errors = 0;
static int		 debug = 0;

const struct ipsec_xf authxfs[] = {
	{ "unknown",		AUTHXF_UNKNOWN,		0,	0 },
	{ "none",		AUTHXF_NONE,		0,	0 },
	{ "hmac-md5",		AUTHXF_HMAC_MD5,	16,	0 },
	{ "hmac-ripemd160",	AUTHXF_HMAC_RIPEMD160,	20,	0 },
	{ "hmac-sha1",		AUTHXF_HMAC_SHA1,	20,	0 },
	{ "hmac-sha2-256",	AUTHXF_HMAC_SHA2_256,	32,	0 },
	{ "hmac-sha2-384",	AUTHXF_HMAC_SHA2_384,	48,	0 },
	{ "hmac-sha2-512",	AUTHXF_HMAC_SHA2_512,	64,	0 },
	{ NULL,			0,			0,	0 },
};

const struct ipsec_xf encxfs[] = {
	{ "unknown",		ENCXF_UNKNOWN,		0,	0 },
	{ "none",		ENCXF_NONE,		0,	0 },
	{ "3des-cbc",		ENCXF_3DES_CBC,		24,	24 },
	{ "des-cbc",		ENCXF_DES_CBC,		8,	8 },
	{ "aes",		ENCXF_AES,		16,	32 },
	{ "aesctr",		ENCXF_AESCTR,		16+4,	32+4 },
	{ "blowfish",		ENCXF_BLOWFISH,		5,	56 },
	{ "cast128",		ENCXF_CAST128,		5,	16 },
	{ "null",		ENCXF_NULL,		0,	0 },
	{ "skipjack",		ENCXF_SKIPJACK,		10,	10 },
	{ NULL,			0,			0,	0 },
};

const struct ipsec_xf compxfs[] = {
	{ "unknown",		COMPXF_UNKNOWN,		0,	0 },
	{ "deflate",		COMPXF_DEFLATE,		0,	0 },
	{ "lzs",		COMPXF_LZS,		0,	0 },
	{ NULL,			0,			0,	0 },
};

int			 yyerror(const char *, ...);
int			 yyparse(void);
int			 kw_cmp(const void *, const void *);
int			 lookup(char *);
int			 lgetc(FILE *);
int			 lungetc(int);
int			 findeol(void);
int			 yylex(void);

TAILQ_HEAD(symhead, sym)	 symhead = TAILQ_HEAD_INITIALIZER(symhead);
struct sym {
	TAILQ_ENTRY(sym)	 entries;
	int		 used;
	int		 persist;
	char		*nam;
	char		*val;
};

int			 symset(const char *, const char *, int);
int			 cmdline_symset(char *);
char			*symget(const char *);
int			 atoul(char *, u_long *);
int			 atospi(char *, u_int32_t *);
u_int8_t		 x2i(unsigned char *);
struct ipsec_key	*parsekey(unsigned char *, size_t);
struct ipsec_key	*parsekeyfile(char *);
struct ipsec_addr_wrap	*host(const char *);
struct ipsec_addr_wrap	*host_v4(const char *, int);
struct ipsec_addr_wrap	*host_dns(const char *, int, int);
struct ipsec_addr_wrap	*host_if(const char *, int);
void			 ifa_load(void);
int			 ifa_exists(const char *);
struct ipsec_addr_wrap	*ifa_lookup(const char *ifa_name);
void			 set_ipmask(struct ipsec_addr_wrap *, u_int8_t);
struct ipsec_addr_wrap	*copyhost(const struct ipsec_addr_wrap *);
const struct ipsec_xf	*parse_xf(const char *, const struct ipsec_xf *);
struct ipsec_transforms *copytransforms(const struct ipsec_transforms *);
int			 validate_sa(u_int32_t, u_int8_t,
			     struct ipsec_transforms *, struct ipsec_key *,
			     struct ipsec_key *, u_int8_t);
struct ipsec_rule	*create_sa(u_int8_t, u_int8_t, struct ipsec_addr_wrap *,
			     struct ipsec_addr_wrap *, u_int32_t,
			     struct ipsec_transforms *, struct ipsec_key *,
			     struct ipsec_key *);
struct ipsec_rule	*reverse_sa(struct ipsec_rule *, u_int32_t,
			     struct ipsec_key *, struct ipsec_key *);
struct ipsec_rule	*create_flow(u_int8_t, u_int8_t, struct
			     ipsec_addr_wrap *, struct ipsec_addr_wrap *,
			     struct ipsec_addr_wrap *, struct ipsec_addr_wrap *,
			     u_int8_t, char *, char *, u_int8_t);
struct ipsec_rule	*reverse_rule(struct ipsec_rule *);
struct ipsec_rule	*create_ike(u_int8_t, struct ipsec_addr_wrap *, struct
			     ipsec_addr_wrap *, struct ipsec_addr_wrap *,
			     struct ipsec_addr_wrap *,
			     struct ipsec_transforms *, struct
			     ipsec_transforms *, u_int8_t, u_int8_t, char *,
			     char *, struct ike_auth *);

struct ipsec_transforms *ipsec_transforms;

typedef struct {
	union {
		u_int32_t	 number;
		u_int8_t	 ikemode;
		u_int8_t	 dir;
		u_int8_t	 satype;	/* encapsulating prococol */
		u_int8_t	 proto;		/* encapsulated protocol */
		u_int8_t	 tmode;
		char		*string;
		struct {
			struct ipsec_addr_wrap *src;
			struct ipsec_addr_wrap *dst;
		} hosts;
		struct {
			struct ipsec_addr_wrap *peer;
			struct ipsec_addr_wrap *local;
		} peers;
		struct ipsec_addr_wrap *singlehost;
		struct ipsec_addr_wrap *host;
		struct {
			char *srcid;
			char *dstid;
		} ids;
		char		*id;
		u_int8_t	 type;
		struct ike_auth	 ikeauth;
		struct {
			u_int32_t	spiout;
			u_int32_t	spiin;
		} spis;
		struct {
			struct ipsec_key *keyout;
			struct ipsec_key *keyin;
		} authkeys;
		struct {
			struct ipsec_key *keyout;
			struct ipsec_key *keyin;
		} enckeys;
		struct {
			struct ipsec_key *keyout;
			struct ipsec_key *keyin;
		} keys;
		struct ipsec_transforms *transforms;
		struct ipsec_transforms *mmxfs;
		struct ipsec_transforms *qmxfs;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	FLOW FROM ESP AH IN PEER ON OUT TO SRCID DSTID RSA PSK TCPMD5 SPI
%token	AUTHKEY ENCKEY FILENAME AUTHXF ENCXF ERROR IKE MAIN QUICK PASSIVE
%token	ACTIVE ANY IPIP IPCOMP COMPXF TUNNEL TRANSPORT DYNAMIC
%token	TYPE DENY BYPASS LOCAL PROTO USE ACQUIRE REQUIRE DONTACQ
%token	<v.string>		STRING
%type	<v.string>		string
%type	<v.dir>			dir
%type	<v.satype>		satype
%type	<v.proto>		proto
%type	<v.tmode>		tmode
%type	<v.number>		number
%type	<v.hosts>		hosts
%type	<v.peers>		peers
%type	<v.singlehost>		singlehost
%type	<v.host>		host
%type	<v.ids>			ids
%type	<v.id>			id
%type	<v.spis>		spispec
%type	<v.authkeys>		authkeyspec
%type	<v.enckeys>		enckeyspec
%type	<v.keys>		keyspec
%type	<v.transforms>		transforms
%type	<v.mmxfs>		mmxfs
%type	<v.qmxfs>		qmxfs
%type	<v.ikemode>		ikemode
%type	<v.ikeauth>		ikeauth
%type	<v.type>		type
%%

grammar		: /* empty */
		| grammar '\n'
		| grammar ikerule '\n'
		| grammar flowrule '\n'
		| grammar sarule '\n'
		| grammar tcpmd5rule '\n'
		| grammar varset '\n'
		| grammar error '\n'		{ errors++; }
		;

number		: STRING			{
			unsigned long	ulval;

			if (atoul($1, &ulval) == -1) {
				yyerror("%s is not a number", $1);
				free($1);
				YYERROR;
			}
			if (ulval > UINT_MAX) {
				yyerror("0x%lx out of range", ulval);
				free($1);
				YYERROR;
			}
			$$ = (u_int32_t)ulval;
			free($1);
		}
		;

tcpmd5rule	: TCPMD5 hosts spispec authkeyspec	{
			struct ipsec_rule	*r;

			r = create_sa(IPSEC_TCPMD5, IPSEC_TRANSPORT, $2.src,
			    $2.dst, $3.spiout, NULL, $4.keyout, NULL);
			if (r == NULL)
				YYERROR;
			r->nr = ipsec->rule_nr++;

			if (ipsecctl_add_rule(ipsec, r))
				errx(1, "tcpmd5rule: ipsecctl_add_rule");

			/* Create and add reverse SA rule. */
			if ($3.spiin != 0 || $4.keyin != NULL) {
				r = reverse_sa(r, $3.spiin, $4.keyin, NULL);
				if (r == NULL)
					YYERROR;
				r->nr = ipsec->rule_nr++;

				if (ipsecctl_add_rule(ipsec, r))
					errx(1, "tcpmd5rule: "
					    "ipsecctl_add_rule");
			}
		}
		;

sarule		: satype tmode hosts spispec transforms authkeyspec
		    enckeyspec {
			struct ipsec_rule	*r;

			r = create_sa($1, $2, $3.src, $3.dst, $4.spiout, $5,
			    $6.keyout, $7.keyout);
			if (r == NULL)
				YYERROR;
			r->nr = ipsec->rule_nr++;

			if (ipsecctl_add_rule(ipsec, r))
				errx(1, "sarule: ipsecctl_add_rule");

			/* Create and add reverse SA rule. */
			if ($4.spiin != 0 || $6.keyin || $7.keyin) {
				r = reverse_sa(r, $4.spiin, $6.keyin,
				    $7.keyin);
				if (r == NULL)
					YYERROR;
				r->nr = ipsec->rule_nr++;

				if (ipsecctl_add_rule(ipsec, r))
					errx(1, "sarule: ipsecctl_add_rule");
			}
		}
		;

flowrule	: FLOW satype dir proto hosts peers ids type {
			struct ipsec_rule	*r;

			r = create_flow($3, $4, $5.src, $5.dst, $6.local,
			    $6.peer, $2, $7.srcid, $7.dstid, $8);
			if (r == NULL)
				YYERROR;
			r->nr = ipsec->rule_nr++;

			if (ipsecctl_add_rule(ipsec, r))
				errx(1, "flowrule: ipsecctl_add_rule");

			/* Create and add reverse flow rule. */
			if ($3 == IPSEC_INOUT) {
				r = reverse_rule(r);
				r->nr = ipsec->rule_nr++;

				if (ipsecctl_add_rule(ipsec, r))
					errx(1, "flowrule: ipsecctl_add_rule");
			}
		}
		;

ikerule		: IKE ikemode satype proto hosts peers mmxfs qmxfs ids ikeauth {
			struct ipsec_rule	*r;

			r = create_ike($4, $5.src, $5.dst, $6.local, $6.peer,
			    $7, $8, $3, $2, $9.srcid, $9.dstid, &$10);
			if (r == NULL)
				YYERROR;
			r->nr = ipsec->rule_nr++;

			if (ipsecctl_add_rule(ipsec, r))
				errx(1, "ikerule: ipsecctl_add_rule");
		}
		;

satype		: /* empty */			{ $$ = IPSEC_ESP; }
		| ESP				{ $$ = IPSEC_ESP; }
		| AH				{ $$ = IPSEC_AH; }
		| IPCOMP			{ $$ = IPSEC_IPCOMP; }
		| IPIP				{ $$ = IPSEC_IPIP; }
		;

proto		: /* empty */			{ $$ = 0; }
		| PROTO STRING			{
			struct protoent *p;
			const char *errstr;
			int proto;

			if ((p = getprotobyname($2)) != NULL) {
				$$ = p->p_proto;
			} else {
				errstr = NULL;
				proto = strtonum($2, 1, 255, &errstr);
				if (errstr)
					errx(1, "unknown protocol: %s", $2);
				$$ = proto;
			}

		}
		;

tmode		: /* empty */			{ $$ = IPSEC_TUNNEL; }
		| TUNNEL			{ $$ = IPSEC_TUNNEL; }
		| TRANSPORT			{ $$ = IPSEC_TRANSPORT; }
		;

dir		: /* empty */			{ $$ = IPSEC_INOUT; }
		| IN				{ $$ = IPSEC_IN; }
		| OUT				{ $$ = IPSEC_OUT; }
		;

hosts		: FROM host TO host		{
			$$.src = $2;
			$$.dst = $4;
		}
		| TO host FROM host		{
			$$.src = $4;
			$$.dst = $2;
		}
		;

peers		: /* empty */				{
			$$.peer = NULL;
			$$.local = NULL;
		}
		| PEER singlehost LOCAL singlehost	{
			$$.peer = $2;
			$$.local = $4;
		}
		| LOCAL singlehost PEER singlehost	{
			$$.peer = $4;
			$$.local = $2;
		}
		| PEER singlehost			{
			$$.peer = $2;
			$$.local = NULL;
		}
		| LOCAL singlehost			{
			$$.peer = NULL;
			$$.local = $2;
		}
		;

singlehost	: /* empty */			{ $$ = NULL; }
		| STRING			{
			if (($$ = host($1)) == NULL) {
				free($1);
				yyerror("could not parse host specification");
				YYERROR;
			}
			free($1);
		}
		;

host		: STRING			{
			if (($$ = host($1)) == NULL) {
				free($1);
				yyerror("could not parse host specification");
				YYERROR;
			}
			free($1);
		}
		| STRING '/' number		{
			char	*buf;

			if (asprintf(&buf, "%s/%u", $1, $3) == -1)
				err(1, "host: asprintf");
			free($1);
			if (($$ = host(buf)) == NULL)	{
				free(buf);
				yyerror("could not parse host specification");
				YYERROR;
			}
			free(buf);
		}
		| ANY				{
			struct ipsec_addr_wrap	*ipa;

			ipa = calloc(1, sizeof(struct ipsec_addr_wrap));
			if (ipa == NULL)
				err(1, "host: calloc");

			ipa->af = AF_INET;
			ipa->netaddress = 1;
			if ((ipa->name = strdup("0.0.0.0/0")) == NULL)
				err(1, "host: strdup");
			$$ = ipa;
		}
		;

ids		: /* empty */			{
			$$.srcid = NULL;
			$$.dstid = NULL;
		}
		| SRCID id DSTID id		{
			$$.srcid = $2;
			$$.dstid = $4;
		}
		| SRCID id			{
			$$.srcid = $2;
			$$.dstid = NULL;
		}
		| DSTID id			{
			$$.srcid = NULL;
			$$.dstid = $2;
		}
		;

type		: /* empty */			{
			$$ = TYPE_REQUIRE;
		}
		| TYPE USE			{
			$$ = TYPE_USE;
		}
		| TYPE ACQUIRE			{
			$$ = TYPE_ACQUIRE;
		}
		| TYPE REQUIRE			{
			$$ = TYPE_REQUIRE;
		}
		| TYPE DENY			{
			$$ = TYPE_DENY;
		}
		| TYPE BYPASS			{
			$$ = TYPE_BYPASS;
		}
		| TYPE DONTACQ			{
			$$ = TYPE_DONTACQ;
		}
		;

id		: STRING			{ $$ = $1; }
		;

spispec		: SPI STRING			{
			u_int32_t	 spi;
			char		*p = strchr($2, ':');

			if (p != NULL) {
				*p++ = 0;

				if (atospi(p, &spi) == -1) {
					yyerror("%s is not a valid spi", p);
					free($2);
					YYERROR;
				}
				$$.spiin = spi;
			}
			if (atospi($2, &spi) == -1) {
				yyerror("%s is not a valid spi", $2);
				free($2);
				YYERROR;
			}
			$$.spiout = spi;


			free($2);
		}
		;

transforms	:					{
			if ((ipsec_transforms = calloc(1,
			    sizeof(struct ipsec_transforms))) == NULL)
				err(1, "transforms: calloc");
		}
		    transforms_l			
			{ $$ = ipsec_transforms; }
		| /* empty */				{
			if (($$ = calloc(1,
			    sizeof(struct ipsec_transforms))) == NULL)
				err(1, "transforms: calloc");
		}
		;

transforms_l	: transforms_l transform
		| transform
		;

transform	: AUTHXF STRING			{
			if (ipsec_transforms->authxf)
				yyerror("auth already set");
			else {
				ipsec_transforms->authxf = parse_xf($2,
				    authxfs);
				if (!ipsec_transforms->authxf)
					yyerror("%s not a valid transform", $2);
			}
		}
		| ENCXF STRING			{
			if (ipsec_transforms->encxf)
				yyerror("enc already set");
			else {
				ipsec_transforms->encxf = parse_xf($2, encxfs);
				if (!ipsec_transforms->encxf)
					yyerror("%s not a valid transform", $2);
			}
		}
		| COMPXF STRING			{
			if (ipsec_transforms->compxf)
				yyerror("comp already set");
			else {
				ipsec_transforms->compxf = parse_xf($2,
				    compxfs);
				if (!ipsec_transforms->compxf)
					yyerror("%s not a valid transform", $2);
			}
		}
		;

mmxfs		: /* empty */			{
			struct ipsec_transforms *xfs;

			/* We create just an empty transform */
			if ((xfs = calloc(1, sizeof(struct ipsec_transforms)))
			    == NULL)
				err(1, "mmxfs: calloc");
			$$ = xfs;
		}
		| MAIN transforms		{ $$ = $2; }
		;

qmxfs		: /* empty */			{
			struct ipsec_transforms *xfs;

			/* We create just an empty transform */
			if ((xfs = calloc(1, sizeof(struct ipsec_transforms)))
			    == NULL)
				err(1, "qmxfs: calloc");
			$$ = xfs;
		}
		| QUICK transforms		{ $$ = $2; }
		;

authkeyspec	: /* empty */			{
			$$.keyout = NULL;
			$$.keyin = NULL;
		}
		| AUTHKEY keyspec		{
			$$.keyout = $2.keyout;
			$$.keyin = $2.keyin;
		}
		;

enckeyspec	: /* empty */			{
			$$.keyout = NULL;
			$$.keyin = NULL;
		}
		| ENCKEY keyspec		{
			$$.keyout = $2.keyout;
			$$.keyin = $2.keyin;
		}
		;

keyspec		: STRING			{
			unsigned char	*hex;
			unsigned char	*p = strchr($1, ':');

			if (p != NULL ) {
				*p++ = 0;

				if (!strncmp(p, "0x", 2))
					p += 2;
				$$.keyin = parsekey(p, strlen(p));
			}

			hex = $1;
			if (!strncmp(hex, "0x", 2))
				hex += 2;
			$$.keyout = parsekey(hex, strlen(hex));

			free($1);
		}
		| FILENAME STRING		{
			unsigned char	*p = strchr($2, ':');

			if (p != NULL) {
				*p++ = 0;
				$$.keyin = parsekeyfile(p);
			}
			$$.keyout = parsekeyfile($2);
			free($2);
		}
		;

ikemode		: /* empty */			{ $$ = IKE_ACTIVE; }
		| PASSIVE			{ $$ = IKE_PASSIVE; }
		| DYNAMIC			{ $$ = IKE_DYNAMIC; }
		| ACTIVE			{ $$ = IKE_ACTIVE; }
		;

ikeauth		: /* empty */			{
			$$.type = IKE_AUTH_RSA;
			$$.string = NULL;
		}
		| RSA				{
			$$.type = IKE_AUTH_RSA;
			$$.string = NULL;
		}
		| PSK STRING			{
			$$.type = IKE_AUTH_PSK;
			if (($$.string = strdup($2)) == NULL)
				err(1, "ikeauth: strdup");
		}
		;

string		: string STRING
		{
			if (asprintf(&$$, "%s %s", $1, $2) == -1)
				err(1, "string: asprintf");
			free($1);
			free($2);
		}
		| STRING
		;

varset		: STRING '=' string
		{
			if (ipsec->opts & IPSECCTL_OPT_VERBOSE)
				printf("%s = \"%s\"\n", $1, $3);
			if (symset($1, $3, 0) == -1)
				err(1, "cannot store variable");
			free($1);
			free($3);
		}
		;

%%

struct keywords {
	const char	*k_name;
	int		 k_val;
};

int
yyerror(const char *fmt, ...)
{
	va_list		 ap;
	extern char	*infile;

	errors = 1;
	va_start(ap, fmt);
	fprintf(stderr, "%s: %d: ", infile, yyval.lineno);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
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
		{ "acquire",		ACQUIRE },
		{ "active",		ACTIVE },
		{ "ah",			AH },
		{ "any",		ANY },
		{ "auth",		AUTHXF },
		{ "authkey",		AUTHKEY },
		{ "bypass",		BYPASS },
		{ "comp",		COMPXF },
		{ "deny",		DENY },
		{ "dontacq",		DONTACQ },
		{ "dstid",		DSTID },
		{ "dynamic",		DYNAMIC },
		{ "enc",		ENCXF },
		{ "enckey",		ENCKEY },
		{ "esp",		ESP },
		{ "file",		FILENAME },
		{ "flow",		FLOW },
		{ "from",		FROM },
		{ "ike",		IKE },
		{ "in",			IN },
		{ "ipcomp",		IPCOMP },
		{ "ipip",		IPIP },
		{ "local",		LOCAL },
		{ "main",		MAIN },
		{ "out",		OUT },
		{ "passive",		PASSIVE },
		{ "peer",		PEER },
		{ "proto",		PROTO },
		{ "psk",		PSK },
		{ "quick",		QUICK },
		{ "require",		REQUIRE },
		{ "rsa",		RSA },
		{ "spi",		SPI },
		{ "srcid",		SRCID },
		{ "tcpmd5",		TCPMD5 },
		{ "to",			TO },
		{ "transport",		TRANSPORT },
		{ "tunnel",		TUNNEL },
		{ "type",		TYPE },
		{ "use",		USE }
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
	int	 endc, c;
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
		} while ((c = lgetc(fin)) != EOF && (allowed_in_string(c)));
		lungetc(c);
		*p = '\0';
		if ((token = lookup(buf)) == STRING)
			if ((yylval.v.string = strdup(buf)) == NULL)
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
parse_rules(FILE *input, struct ipsecctl *ipsecx)
{
	struct sym	*sym;

	ipsec = ipsecx;
	fin = input;
	lineno = 1;
	errors = 0;

	yyparse();

	/* Free macros and check which have not been used. */
	while ((sym = TAILQ_FIRST(&symhead))) {
		if ((ipsec->opts & IPSECCTL_OPT_VERBOSE2) && !sym->used)
			fprintf(stderr, "warning: macro '%s' not "
			    "used\n", sym->nam);
		TAILQ_REMOVE(&symhead, sym, entries);
		free(sym->nam);
		free(sym->val);
		free(sym);
	}

	return (errors ? -1 : 0);
}

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
			TAILQ_REMOVE(&symhead, sym, entries);
			free(sym->nam);
			free(sym->val);
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
cmdline_symset(char *s)
{
	char	*sym, *val;
	int	ret;
	size_t	len;

	if ((val = strrchr(s, '=')) == NULL)
		return (-1);

	len = strlen(s) - strlen(val) + 1;
	if ((sym = malloc(len)) == NULL)
		err(1, "cmdline_symset: malloc");

	strlcpy(sym, s, len);

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
atospi(char *s, u_int32_t *spivalp)
{
	unsigned long	ulval;

	if (atoul(s, &ulval) == -1)
		return (-1);
	if (ulval >= SPI_RESERVED_MIN && ulval <= SPI_RESERVED_MAX) {
		yyerror("illegal SPI value");
		return (-1);
	}
	*spivalp = ulval;
	return (0);
}

u_int8_t
x2i(unsigned char *s)
{
	char	ss[3];

	ss[0] = s[0];
	ss[1] = s[1];
	ss[2] = 0;

	if (!isxdigit(s[0]) || !isxdigit(s[1])) {
		yyerror("keys need to be specified in hex digits");
		return (-1);
	}
	return ((u_int8_t)strtoul(ss, NULL, 16));
}

struct ipsec_key *
parsekey(unsigned char *hexkey, size_t len)
{
	struct ipsec_key *key;
	int		  i;

	key = calloc(1, sizeof(struct ipsec_key));
	if (key == NULL)
		err(1, "parsekey: calloc");

	key->len = len / 2;
	key->data = calloc(key->len, sizeof(u_int8_t));
	if (key->data == NULL)
		err(1, "parsekey: calloc");

	for (i = 0; i < (int)key->len; i++)
		key->data[i] = x2i(hexkey + 2 * i);

	return (key);
}

struct ipsec_key *
parsekeyfile(char *filename)
{
	struct stat	 sb;
	int		 fd;
	unsigned char	*hex;

	if ((fd = open(filename, O_RDONLY)) < 0)
		err(1, "parsekeyfile: open");
	if (fstat(fd, &sb) < 0)
		err(1, "parsekeyfile: stat %s", filename);
	if ((sb.st_size > KEYSIZE_LIMIT) || (sb.st_size == 0))
		errx(1, "parsekeyfile: key too %s", sb.st_size ? "large" :
		    "small");
	if ((hex = calloc(sb.st_size, sizeof(unsigned char))) == NULL)
		err(1, "parsekeyfile: calloc");
	if (read(fd, hex, sb.st_size) < sb.st_size)
		err(1, "parsekeyfile: read");
	close(fd);
	return (parsekey(hex, sb.st_size));
}

struct ipsec_addr_wrap *
host(const char *s)
{
	struct ipsec_addr_wrap	*ipa = NULL;
	int			 mask, v4mask, cont = 1;
	char			*p, *q, *ps;

	if ((p = strrchr(s, '/')) != NULL) {
		mask = strtol(p + 1, &q, 0);
		if (!q || *q || mask > 32 || q == (p + 1))
			errx(1, "host: invalid netmask '%s'", p);
		if ((ps = malloc(strlen(s) - strlen(p) + 1)) == NULL)
			err(1, "host: calloc");
		strlcpy(ps, s, strlen(s) - strlen(p) + 1);
		v4mask = mask;
	} else {
		if ((ps = strdup(s)) == NULL)
			err(1, "host: strdup");
		v4mask = 32;
		mask = -1;
	}

	/* Does interface with this name exist? */
	if (cont && (ipa = host_if(ps, mask)) != NULL)
		cont = 0;

	/* IPv4 address? */
	if (cont && (ipa = host_v4(s, v4mask)) != NULL)
		cont = 0;

#if notyet
	/* IPv6 address? */
	if (cont && (ipa = host_v6(s, v6mask)) != NULL)
		cont = 0;
#endif
	
	/* dns lookup */
	if (cont && (ipa = host_dns(s, v4mask, 0)) != NULL)
		cont = 0;
	free(ps);

	if (ipa == NULL || cont == 1) {
		fprintf(stderr, "no IP address found for %s\n", s);
		return (NULL);
	}
	return (ipa);
}

struct ipsec_addr_wrap *
host_v4(const char *s, int mask)
{
	struct ipsec_addr_wrap	*ipa = NULL;
	struct in_addr		 ina;
	int			 bits = 32;

	bzero(&ina, sizeof(struct in_addr));
	if (strrchr(s, '/') != NULL) {
		if ((bits = inet_net_pton(AF_INET, s, &ina, sizeof(ina))) == -1)
			return (NULL);
	} else {
		if (inet_pton(AF_INET, s, &ina) != 1)
			return (NULL);
	}

	ipa = calloc(1, sizeof(struct ipsec_addr_wrap));
	if (ipa == NULL)
		err(1, "host_v4: calloc");

	ipa->address.v4 = ina;
	ipa->name = strdup(s);
	if (ipa->name == NULL)
		err(1, "host_v4: strdup");
	ipa->af = AF_INET;

	set_ipmask(ipa, bits);
	if (bits != (ipa->af == AF_INET ? 32 : 128))
		ipa->netaddress = 1;

	return (ipa);
}

struct ipsec_addr_wrap *
host_dns(const char *s, int v4mask, int v6mask)
{
	struct ipsec_addr_wrap	*ipa = NULL;
	struct addrinfo	 	 hints, *res0, *res;
	int		 	 error;
	int			 bits = 32;

	bzero(&hints, sizeof(struct addrinfo));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(s, NULL, &hints, &res0);
	if (error)
		return (NULL);

	for (res = res0; res; res = res->ai_next) {
		if (res->ai_family != AF_INET)
			continue;
		ipa = calloc(1, sizeof(struct ipsec_addr_wrap));
		if (ipa == NULL)
			err(1, "host_dns: calloc");
		memcpy(&ipa->address.v4,
		    &((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr,
		    sizeof(struct in_addr));
		ipa->name = strdup(inet_ntoa(ipa->address.v4));
		if (ipa->name == NULL)
			err(1, "host_dns: strdup");
		ipa->af = AF_INET;

		set_ipmask(ipa, bits);
		if (bits != (ipa->af == AF_INET ? 32 : 128))
			ipa->netaddress = 1;
		break;
	}
	freeaddrinfo(res0);

	return (ipa);
}

struct ipsec_addr_wrap *
host_if(const char *s, int mask)
{
	struct ipsec_addr_wrap *ipa = NULL;

	if (ifa_exists(s))
		ipa = ifa_lookup(s);

	return (ipa);
}

/* interface lookup routintes */

struct addr_node	*iftab;

void
ifa_load(void)
{
	struct ifaddrs		*ifap, *ifa;
	struct addr_node	*n = NULL, *h = NULL;

	if (getifaddrs(&ifap) < 0)
		err(1, "ifa_load: getiffaddrs");

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (!(ifa->ifa_addr->sa_family == AF_INET ||
		    ifa->ifa_addr->sa_family == AF_INET6 ||
		    ifa->ifa_addr->sa_family == AF_LINK))
			continue;
		n = calloc(1, sizeof(struct addr_node));
		if (n == NULL)
			err(1, "ifa_load: calloc");
		n->af = ifa->ifa_addr->sa_family;
		if ((n->addr.name = strdup(ifa->ifa_name)) == NULL)
			err(1, "ifa_load: strdup");
		if (n->af == AF_INET) {
			n->addr.af = AF_INET;
			memcpy(&n->addr.address.v4, &((struct sockaddr_in *)
			    ifa->ifa_addr)->sin_addr.s_addr,
			    sizeof(struct in_addr));
			memcpy(&n->addr.mask.v4, &((struct sockaddr_in *)
			    ifa->ifa_netmask)->sin_addr.s_addr,
			    sizeof(struct in_addr));
		} else if (n->af == AF_INET6) {
			n->addr.af = AF_INET6;
			memcpy(&n->addr.address.v6, &((struct sockaddr_in6 *)
			    ifa->ifa_addr)->sin6_addr.s6_addr,
			    sizeof(struct in6_addr));
			memcpy(&n->addr.mask.v6, &((struct sockaddr_in6 *)
			    ifa->ifa_netmask)->sin6_addr.s6_addr,
			    sizeof(struct in6_addr));
		}
		if ((n->addr.name = strdup(ifa->ifa_name)) == NULL)
			err(1, "ifa_load: strdup");
		n->next = NULL;
		n->tail = n;
		if (h == NULL)
			h = n;
		else {
			h->tail->next = n;
			h->tail = n;
		}
	}

	iftab = h;
	freeifaddrs(ifap);
}

int
ifa_exists(const char *ifa_name)
{
	struct addr_node	*n;

	if (iftab == NULL)
		ifa_load();

	for (n = iftab; n; n = n->next) {
		if (n->af == AF_LINK && !strncmp(n->addr.name, ifa_name,
		    IFNAMSIZ))
			return (1);
	}

	return (0);
}

struct ipsec_addr_wrap *
ifa_lookup(const char *ifa_name)
{
	struct addr_node	*p = NULL;
	struct ipsec_addr_wrap	*ipa = NULL;

	if (iftab == NULL)
		ifa_load();

	for (p = iftab; p; p = p->next) {
		if (p->af != AF_INET)
			continue;
		if (strncmp(p->addr.name, ifa_name, IFNAMSIZ))
			continue;
		ipa = calloc(1, sizeof(struct ipsec_addr_wrap));
		if (ipa == NULL)
			err(1, "ifa_lookup: calloc");
		memcpy(ipa, &p->addr, sizeof(struct ipsec_addr_wrap));
		if ((ipa->name = strdup(p->addr.name)) == NULL)
			err(1, "ifa_lookup: strdup");
		set_ipmask(ipa, 32);
		break;
	}

	return (ipa);
}

void
set_ipmask(struct ipsec_addr_wrap *address, u_int8_t b)
{
	struct ipsec_addr	*ipa;
	int			 i, j = 0;

	ipa = &address->mask;
	bzero(ipa, sizeof(struct ipsec_addr));

	while (b >= 32) {
		ipa->addr32[j++] = 0xffffffff;
		b -= 32;
	}
	for (i = 31; i > 31 - b; --i)
		ipa->addr32[j] |= (1 << i);
	if (b)
		ipa->addr32[j] = htonl(ipa->addr32[j]);
}

struct ipsec_addr_wrap *
copyhost(const struct ipsec_addr_wrap *src)
{
	struct ipsec_addr_wrap *dst;

	dst = calloc(1, sizeof(struct ipsec_addr_wrap));
	if (dst == NULL)
		err(1, "copyhost: calloc");

	memcpy(dst, src, sizeof(struct ipsec_addr_wrap));

	if ((dst->name = strdup(src->name)) == NULL)
		err(1, "copyhost: strdup");

	return dst;
}

const struct ipsec_xf *
parse_xf(const char *name, const struct ipsec_xf xfs[])
{
	int		i;

	for (i = 0; xfs[i].name != NULL; i++) {
		if (strncmp(name, xfs[i].name, strlen(name)))
			continue;
		return &xfs[i];
	}
	return (NULL);
}

struct ipsec_transforms *
copytransforms(const struct ipsec_transforms *xfs)
{
	struct ipsec_transforms *newxfs;

	if (xfs == NULL)
		return (NULL);

	newxfs = calloc(1, sizeof(struct ipsec_transforms));
	if (newxfs == NULL)
		err(1, "copytransforms: calloc");

	memcpy(newxfs, xfs, sizeof(struct ipsec_transforms));
	return (newxfs);
}

int
validate_sa(u_int32_t spi, u_int8_t satype, struct ipsec_transforms *xfs,
    struct ipsec_key *authkey, struct ipsec_key *enckey, u_int8_t tmode)
{
	/* Sanity checks */
	if (spi == 0) {
		yyerror("no SPI specified");
		return (0);
	}
	if (satype == IPSEC_AH) {
		if (!xfs) {
			yyerror("no transforms specified");
			return (0);
		}
		if (!xfs->authxf)
			xfs->authxf = &authxfs[AUTHXF_HMAC_SHA2_256];
		if (xfs->encxf) {
			yyerror("ah does not provide encryption");
			return (0);
		}
		if (xfs->compxf) {
			yyerror("ah does not provide compression");
			return (0);
		}
	}
	if (satype == IPSEC_ESP) {
		if (!xfs) {
			yyerror("no transforms specified");
			return (0);
		}
		if (xfs->compxf) {
			yyerror("esp does not provide compression");
			return (0);
		}
		if (!xfs->authxf)
			xfs->authxf = &authxfs[AUTHXF_HMAC_SHA2_256];
		if (!xfs->encxf)
			xfs->encxf = &encxfs[ENCXF_AESCTR];
	}
	if (satype == IPSEC_IPCOMP) {
		if (!xfs) {
			yyerror("no transform specified");
			return (0);
		}
		if (xfs->authxf || xfs->encxf) {
			yyerror("no encryption or authenticaion with ipcomp");
			return (0);
		}
		if (!xfs->compxf)
			xfs->compxf = &compxfs[COMPXF_DEFLATE];
	}
	if (satype == IPSEC_IPIP) {
		if (!xfs) {
			yyerror("no transform specified");
			return (0);
		}
		if (xfs->authxf || xfs->encxf || xfs->compxf) {
			yyerror("no encryption, authenticaion or compression"
			    " with ipip");
			return (0);
		}
	}
	if (satype == IPSEC_TCPMD5 && authkey == NULL && tmode !=
	    IPSEC_TRANSPORT) {
		yyerror("authentication key needed for tcpmd5");
		return (0);
	}
	if (xfs && xfs->authxf) {
		if (!authkey) {
			yyerror("no authentication key specified");
			return (0);
		}
		if (authkey->len != xfs->authxf->keymin) {
			yyerror("wrong authentication key length, needs to be "
			    "%d bits", xfs->authxf->keymin * 8);
			return (0);
		}
	}
	if (xfs && xfs->encxf) {
		if (!enckey && xfs->encxf != &encxfs[ENCXF_NULL]) {
			yyerror("no encryption key specified");
			return (0);
		}
		if (enckey) {
			if (enckey->len < xfs->encxf->keymin) {
				yyerror("encryption key too short, "
				    "minimum %d bits", xfs->encxf->keymin * 8);
				return (0);
			}
			if (xfs->encxf->keymax < enckey->len) {
				yyerror("encryption key too long, "
				    "maximum %d bits", xfs->encxf->keymax * 8);
				return (0);
			}
		}
	}

	return 1;
}

struct ipsec_rule *
create_sa(u_int8_t satype, u_int8_t tmode, struct ipsec_addr_wrap *src, struct
    ipsec_addr_wrap *dst, u_int32_t spi, struct ipsec_transforms *xfs,
    struct ipsec_key *authkey, struct ipsec_key *enckey)
{
	struct ipsec_rule *r;

	if (validate_sa(spi, satype, xfs, authkey, enckey, tmode) == 0)
		return (NULL);

	r = calloc(1, sizeof(struct ipsec_rule));
	if (r == NULL)
		err(1, "create_sa: calloc");

	r->type |= RULE_SA;
	r->satype = satype;
	r->tmode = tmode;
	r->src = src;
	r->dst = dst;
	r->spi = spi;
	r->xfs = xfs;
	r->authkey = authkey;
	r->enckey = enckey;

	return r;
}

struct ipsec_rule *
reverse_sa(struct ipsec_rule *rule, u_int32_t spi, struct ipsec_key *authkey,
    struct ipsec_key *enckey)
{
	struct ipsec_rule *reverse;

	if (validate_sa(spi, rule->satype, rule->xfs, authkey, enckey,
	    rule->tmode) == 0)
		return (NULL);

	reverse = calloc(1, sizeof(struct ipsec_rule));
	if (reverse == NULL)
		err(1, "reverse_sa: calloc");

	reverse->type |= RULE_SA;
	reverse->satype = rule->satype;
	reverse->tmode = rule->tmode;
	reverse->src = copyhost(rule->dst);
	reverse->dst = copyhost(rule->src);
	reverse->spi = spi;
	reverse->xfs = copytransforms(rule->xfs);
	reverse->authkey = authkey;
	reverse->enckey = enckey;

	return (reverse);
}

struct ipsec_rule *
create_flow(u_int8_t dir, u_int8_t proto, struct ipsec_addr_wrap *src,
    struct ipsec_addr_wrap *dst, struct ipsec_addr_wrap *local,
    struct ipsec_addr_wrap *peer, u_int8_t satype, char *srcid, char *dstid,
    u_int8_t type)
{
	struct ipsec_rule *r;

	r = calloc(1, sizeof(struct ipsec_rule));
	if (r == NULL)
		err(1, "create_flow: calloc");

	r->type |= RULE_FLOW;

	if (dir == IPSEC_INOUT)
		r->direction = IPSEC_OUT;
	else
		r->direction = dir;

	r->satype = satype;
	r->proto = proto;
	r->src = src;
	r->dst = dst;

	if (type == TYPE_DENY || type == TYPE_BYPASS) {
		r->flowtype = type;
		return (r);
	}

	r->flowtype = type;
	r->local = local;
	if (peer == NULL) {
		/* Set peer to remote host.  Must be a host address. */
		if (r->direction == IPSEC_IN) {
			if (r->src->netaddress) {
				yyerror("no peer specified");
				goto errout;
			}
			r->peer = copyhost(r->src);
		} else {
			if (r->dst->netaddress) {
				yyerror("no peer specified");
				goto errout;
			}
			r->peer = copyhost(r->dst);
		}
	} else
		r->peer = peer;

	r->auth = calloc(1, sizeof(struct ipsec_auth));
	if (r->auth == NULL)
		err(1, "create_flow: calloc");
	r->auth->srcid = srcid;
	r->auth->dstid = dstid;
	r->auth->idtype = ID_FQDN;	/* XXX For now only FQDN. */

	return r;

errout:
	free(r);
	if (srcid)
		free(srcid);
	if (dstid)
		free(dstid);
	free(src);
	free(dst);

	return NULL;
}

struct ipsec_rule *
reverse_rule(struct ipsec_rule *rule)
{
	struct ipsec_rule *reverse;

	reverse = calloc(1, sizeof(struct ipsec_rule));
	if (reverse == NULL)
		err(1, "reverse_rule: calloc");

	reverse->type |= RULE_FLOW;

	/* Reverse direction */
	if (rule->direction == (u_int8_t)IPSEC_OUT)
		reverse->direction = (u_int8_t)IPSEC_IN;
	else
		reverse->direction = (u_int8_t)IPSEC_OUT;

	reverse->flowtype = rule->flowtype;
	reverse->src = copyhost(rule->dst);
	reverse->dst = copyhost(rule->src);
	if (rule->local)
		reverse->local = copyhost(rule->local);
	if (rule->peer)
		reverse->peer = copyhost(rule->peer);
	reverse->satype = rule->satype;
	reverse->proto = rule->proto;

	if (rule->auth) {
		reverse->auth = calloc(1, sizeof(struct ipsec_auth));
		if (reverse->auth == NULL)
			err(1, "reverse_rule: calloc");
		if (rule->auth->dstid && (reverse->auth->dstid =
		    strdup(rule->auth->dstid)) == NULL)
			err(1, "reverse_rule: strdup");
		if (rule->auth->srcid && (reverse->auth->srcid =
		    strdup(rule->auth->srcid)) == NULL)
			err(1, "reverse_rule: strdup");
		reverse->auth->idtype = rule->auth->idtype;
		reverse->auth->type = rule->auth->type;
	}

	return reverse;
}

struct ipsec_rule *
create_ike(u_int8_t proto, struct ipsec_addr_wrap *src, struct ipsec_addr_wrap
    *dst, struct ipsec_addr_wrap *local, struct ipsec_addr_wrap *peer,
    struct ipsec_transforms *mmxfs, struct ipsec_transforms *qmxfs,
    u_int8_t satype, u_int8_t mode, char *srcid, char *dstid,
    struct ike_auth *authtype)
{
	struct ipsec_rule *r;

	r = calloc(1, sizeof(struct ipsec_rule));
	if (r == NULL)
		err(1, "create_ike: calloc");

	r->type = RULE_IKE;

	r->proto = proto;
	r->src = src;
	r->dst = dst;

	if (peer == NULL) {
		/* Set peer to remote host.  Must be a host address. */
		if (r->direction == IPSEC_IN) {
			if (r->src->netaddress) {
				yyerror("no peer specified");
				goto errout;
			}
			r->peer = copyhost(r->src);
		} else {
			if (r->dst->netaddress) {
				yyerror("no peer specified");
				goto errout;
			}
			r->peer = copyhost(r->dst);
		}
	} else
		r->peer = peer;

	if (local)
		r->local = local;

	r->satype = satype;
	r->ikemode = mode;
	r->mmxfs = mmxfs;
	r->qmxfs = qmxfs;
	r->auth = calloc(1, sizeof(struct ipsec_auth));
	if (r->auth == NULL)
		err(1, "create_ike: calloc");
	r->auth->srcid = srcid;
	r->auth->dstid = dstid;
	r->auth->idtype = ID_FQDN;	/* XXX For now only FQDN. */
	r->ikeauth = calloc(1, sizeof(struct ike_auth));
	if (r->ikeauth == NULL)
		err(1, "create_ike: calloc");
	r->ikeauth->type = authtype->type;
	r->ikeauth->string = authtype->string;

	return (r);

errout:
	free(r);
	if (srcid)
		free(srcid);
	if (dstid)
		free(dstid);
	free(src);
	free(dst);
	if (authtype->string)
		free(authtype->string);

	return (NULL);
}
