/*	$OpenBSD: parse.y,v 1.91 2012/08/21 20:19:46 eric Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2002, 2003, 2004 Henning Brauer <henning@openbsd.org>
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
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <ifaddrs.h>
#include <imsg.h>
#include <inttypes.h>
#include <netdb.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "smtpd.h"
#include "log.h"

TAILQ_HEAD(files, file)		 files = TAILQ_HEAD_INITIALIZER(files);
static struct file {
	TAILQ_ENTRY(file)	 entry;
	FILE			*stream;
	char			*name;
	int			 lineno;
	int			 errors;
} *file, *topfile;
struct file	*pushfile(const char *, int);
int		 popfile(void);
int		 check_file_secrecy(int, const char *);
int		 yyparse(void);
int		 yylex(void);
int		 kw_cmp(const void *, const void *);
int		 lookup(char *);
int		 lgetc(int);
int		 lungetc(int);
int		 findeol(void);
int		 yyerror(const char *, ...)
    __attribute__ ((format (printf, 1, 2)));

TAILQ_HEAD(symhead, sym)	 symhead = TAILQ_HEAD_INITIALIZER(symhead);
struct sym {
	TAILQ_ENTRY(sym)	 entry;
	int			 used;
	int			 persist;
	char			*nam;
	char			*val;
};
int		 symset(const char *, const char *, int);
char		*symget(const char *);

struct smtpd		*conf = NULL;
static int		 errors = 0;

objid_t			 last_map_id = 1;
struct map		*map = NULL;
struct rule		*rule = NULL;
TAILQ_HEAD(condlist, cond) *conditions = NULL;
struct mapel_list	*contents = NULL;

struct listener	*host_v4(const char *, in_port_t);
struct listener	*host_v6(const char *, in_port_t);
int		 host_dns(const char *, const char *, const char *,
		    struct listenerlist *, int, in_port_t, uint8_t);
int		 host(const char *, const char *, const char *,
		    struct listenerlist *, int, in_port_t, uint8_t);
int		 interface(const char *, const char *, const char *,
		    struct listenerlist *, int, in_port_t, uint8_t);
void		 set_localaddrs(void);
int		 delaytonum(char *);
int		 is_if_in_group(const char *, const char *);

typedef struct {
	union {
		int64_t		 number;
		objid_t		 object;
		struct timeval	 tv;
		struct cond	*cond;
		char		*string;
		struct host	*host;
		struct mailaddr	*maddr;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	AS QUEUE COMPRESS INTERVAL SIZE LISTEN ON ALL PORT EXPIRE
%token	MAP HASH LIST SINGLE SSL SMTPS CERTIFICATE
%token	DB PLAIN DOMAIN SOURCE
%token  RELAY BACKUP VIA DELIVER TO MAILDIR MBOX HOSTNAME
%token	ACCEPT REJECT INCLUDE ERROR MDA FROM FOR
%token	ARROW ENABLE AUTH TLS LOCAL VIRTUAL TAG ALIAS FILTER
%token	<v.string>	STRING
%token  <v.number>	NUMBER
%type	<v.map>		map
%type	<v.number>	quantifier decision port from auth ssl size expire
%type	<v.cond>	condition
%type	<v.tv>		interval
%type	<v.object>	mapref
%type	<v.maddr>	relay_as
%type	<v.string>	certname user tag on alias credentials compress

%%

grammar		: /* empty */
		| grammar '\n'
		| grammar include '\n'
		| grammar varset '\n'
		| grammar main '\n'
		| grammar map '\n'
		| grammar rule '\n'
		| grammar error '\n'		{ file->errors++; }
		;

include		: INCLUDE STRING		{
			struct file	*nfile;

			if ((nfile = pushfile($2, 0)) == NULL) {
				yyerror("failed to include file %s", $2);
				free($2);
				YYERROR;
			}
			free($2);

			file = nfile;
			lungetc('\n');
		}
		;

varset		: STRING '=' STRING		{
			if (symset($1, $3, 0) == -1)
				fatal("cannot store variable");
			free($1);
			free($3);
		}
		;

comma		: ','
		| nl
		| /* empty */
		;

optnl		: '\n' optnl
		|
		;

optlbracket    	: '{'
		|
		;

optrbracket    	: '}'
		|
		;

nl		: '\n' optnl
		;

quantifier      : /* empty */                   { $$ = 1; }  	 
		| 'm'                           { $$ = 60; } 	 
		| 'h'                           { $$ = 3600; } 	 
		| 'd'                           { $$ = 86400; } 	 
		;

interval	: NUMBER quantifier		{
			if ($1 < 0) {
				yyerror("invalid interval: %" PRId64, $1);
				YYERROR;
			}
			$$.tv_usec = 0;
			$$.tv_sec = $1 * $2;
		}
		;

size		: NUMBER		{
			if ($1 < 0) {
				yyerror("invalid size: %" PRId64, $1);
				YYERROR;
			}
			$$ = $1;
		}
		| STRING			{
			long long result;

			if (scan_scaled($1, &result) == -1 || result < 0) {
				yyerror("invalid size: %s", $1);
				YYERROR;
			}
			free($1);

			$$ = result;
		}
		;

port		: PORT STRING			{
			struct servent	*servent;

			servent = getservbyname($2, "tcp");
			if (servent == NULL) {
				yyerror("port %s is invalid", $2);
				free($2);
				YYERROR;
			}
			$$ = servent->s_port;
			free($2);
		}
		| PORT NUMBER			{
			if ($2 <= 0 || $2 >= (int)USHRT_MAX) {
				yyerror("invalid port: %" PRId64, $2);
				YYERROR;
			}
			$$ = htons($2);
		}
		| /* empty */			{
			$$ = 0;
		}
		;

certname	: CERTIFICATE STRING	{
			if (($$ = strdup($2)) == NULL)
				fatal(NULL);
			free($2);
		}
		| /* empty */			{ $$ = NULL; }
		;

ssl		: SMTPS				{ $$ = F_SMTPS; }
		| TLS				{ $$ = F_STARTTLS; }
		| SSL				{ $$ = F_SSL; }
		| /* empty */			{ $$ = 0; }
		;

auth		: ENABLE AUTH  			{ $$ = 1; }
		| /* empty */			{ $$ = 0; }
		;

tag		: TAG STRING			{
       			if (strlen($2) >= MAX_TAG_SIZE) {
       				yyerror("tag name too long");
				free($2);
				YYERROR;
			}

			$$ = $2;
		}
		| /* empty */			{ $$ = NULL; }
		;

expire		: EXPIRE STRING {
			$$ = delaytonum($2);
			if ($$ == -1) {
				yyerror("invalid expire delay: %s", $2);
				YYERROR;
			}
			free($2);
		}
		| /* empty */	{ $$ = conf->sc_qexpire; }
		;

credentials	: AUTH STRING	{
			struct map *m;

			if ((m = map_findbyname($2)) == NULL) {
				yyerror("no such map: %s", $2);
				free($2);
				YYERROR;
			}
			$$ = $2;
		}
		| /* empty */	{ $$ = 0; }
		;

compress	: COMPRESS STRING {
			$$ = $2;
		}
		| COMPRESS {
			$$ = "zlib";
		}
		| /* empty */	{ $$ = NULL; }
		;

main		: QUEUE INTERVAL interval	{
			conf->sc_qintval = $3;
		}
		| QUEUE compress {
			if ($2) {
				conf->sc_queue_flags |= QUEUE_COMPRESS;
				conf->sc_queue_compress_algo = strdup($2);
				log_debug("queue compress using %s", conf->sc_queue_compress_algo);
			}
			if ($2 == NULL) {
				yyerror("invalid queue compress <algo>");
				YYERROR;
			}
		}
		| EXPIRE STRING {
			conf->sc_qexpire = delaytonum($2);
			if (conf->sc_qexpire == -1) {
				yyerror("invalid expire delay: %s", $2);
				YYERROR;
			}
		}
	       	| SIZE size {
       			conf->sc_maxsize = $2;
		}
		| LISTEN ON STRING port ssl certname auth tag {
			char		*cert;
			char		*tag;
			uint8_t		 flags;

			if ($5 == F_SSL) {
				yyerror("syntax error");
				free($8);
				free($6);
				free($3);
				YYERROR;
			}

			if ($5 == 0 && ($6 != NULL || $7)) {
				yyerror("error: must specify tls or smtps");
				free($8);
				free($6);
				free($3);
				YYERROR;
			}

			if ($4 == 0) {
				if ($5 == F_SMTPS)
					$4 = htons(465);
				else
					$4 = htons(25);
			}

			cert = ($6 != NULL) ? $6 : $3;
			flags = $5;

			if ($7)
				flags |= F_AUTH;

			if ($5 && ssl_load_certfile(cert, F_SCERT) < 0) {
				yyerror("cannot load certificate: %s", cert);
				free($8);
				free($6);
				free($3);
				YYERROR;
			}

			tag = $3;
			if ($8 != NULL)
				tag = $8;

			if (! interface($3, tag, cert, conf->sc_listeners,
				MAX_LISTEN, $4, flags)) {
				if (host($3, tag, cert, conf->sc_listeners,
					MAX_LISTEN, $4, flags) <= 0) {
					yyerror("invalid virtual ip or interface: %s", $3);
					free($8);
					free($6);
					free($3);
					YYERROR;
				}
			}
			free($8);
			free($6);
			free($3);
		}
		| HOSTNAME STRING		{
			if (strlcpy(conf->sc_hostname, $2,
			    sizeof(conf->sc_hostname)) >=
			    sizeof(conf->sc_hostname)) {
				yyerror("hostname truncated");
				free($2);
				YYERROR;
			}
			free($2);
		}/*
		| FILTER STRING STRING		{
			struct filter *filter;
			struct filter *tmp;

			filter = calloc(1, sizeof (*filter));
			if (filter == NULL ||
			    strlcpy(filter->name, $2, sizeof (filter->name))
			    >= sizeof (filter->name) ||
			    strlcpy(filter->path, $3, sizeof (filter->path))
			    >= sizeof (filter->path)) {
				free(filter);
				free($2);
				free($3);
				YYERROR;
			}

			TAILQ_FOREACH(tmp, conf->sc_filters, f_entry) {
				if (strcasecmp(filter->name, tmp->name) == 0)
					break;
			}
			if (tmp == NULL)
				TAILQ_INSERT_TAIL(conf->sc_filters, filter, f_entry);
			else {
       				yyerror("ambiguous filter name: %s", filter->name);
				free($2);
				free($3);
				YYERROR;
			}
			free($2);
			free($3);
		}
		*/
		;

mapsource	: PLAIN STRING			{
			map->m_src = S_PLAIN;
			if (strlcpy(map->m_config, $2, sizeof(map->m_config))
			    >= sizeof(map->m_config))
				err(1, "pathname too long");
		}
		| DB STRING			{
			map->m_src = S_DB;
			if (strlcpy(map->m_config, $2, sizeof(map->m_config))
			    >= sizeof(map->m_config))
				err(1, "pathname too long");
		}
		;

mapopt		: SOURCE mapsource		{ }

map		: MAP STRING			{
			struct map	*m;

			TAILQ_FOREACH(m, conf->sc_maps, m_entry)
				if (strcmp(m->m_name, $2) == 0)
					break;

			if (m != NULL) {
				yyerror("map %s defined twice", $2);
				free($2);
				YYERROR;
			}
			if ((m = calloc(1, sizeof(*m))) == NULL)
				fatal("out of memory");
			if (strlcpy(m->m_name, $2, sizeof(m->m_name)) >=
			    sizeof(m->m_name)) {
				yyerror("map name truncated");
				free(m);
				free($2);
				YYERROR;
			}

			m->m_id = last_map_id++;

			if (m->m_id == INT_MAX) {
				yyerror("too many maps defined");
				free($2);
				free(m);
				YYERROR;
			}
			map = m;
		} optlbracket mapopt optrbracket	{
			if (map->m_src == S_NONE) {
				yyerror("map %s has no source defined", $2);
				free(map);
				map = NULL;
				YYERROR;
			}
			TAILQ_INSERT_TAIL(conf->sc_maps, map, m_entry);
			map = NULL;
		}
		;

keyval		: STRING ARROW STRING		{
			struct mapel	*me;

			if ((me = calloc(1, sizeof(*me))) == NULL)
				fatal("out of memory");

			if (strlcpy(me->me_key.med_string, $1,
			    sizeof(me->me_key.med_string)) >=
			    sizeof(me->me_key.med_string) ||
			    strlcpy(me->me_val.med_string, $3,
			    sizeof(me->me_val.med_string)) >=
			    sizeof(me->me_val.med_string)) {
				yyerror("map elements too long: %s, %s",
				    $1, $3);
				free(me);
				free($1);
				free($3);
				YYERROR;
			}
			free($1);
			free($3);

			TAILQ_INSERT_TAIL(contents, me, me_entry);
		}
		;

keyval_list	: keyval
		| keyval comma keyval_list
		;

stringel	: STRING			{
			struct mapel	*me;

			if ((me = calloc(1, sizeof(*me))) == NULL)
				fatal("out of memory");

			if (strlcpy(me->me_key.med_string, $1,
				sizeof(me->me_key.med_string)) >=
			    sizeof(me->me_key.med_string)) {
				yyerror("map element too long: %s", $1);
				free(me);
				free($1);
				YYERROR;
			}
			free($1);
			TAILQ_INSERT_TAIL(contents, me, me_entry);
		}
		;

string_list	: stringel
		| stringel comma string_list
		;

mapref		: STRING			{
			struct map	*m;
			struct mapel	*me;

			if ((m = calloc(1, sizeof(*m))) == NULL)
				fatal("out of memory");
			m->m_id = last_map_id++;
			if (m->m_id == INT_MAX) {
				yyerror("too many maps defined");
				free(m);
				YYERROR;
			}
			if (! bsnprintf(m->m_name, sizeof(m->m_name),
				"<dynamic(%u)>", m->m_id))
				fatal("snprintf");
			m->m_src = S_NONE;

			TAILQ_INIT(&m->m_contents);

			if ((me = calloc(1, sizeof(*me))) == NULL)
				fatal("out of memory");

			if (strlcpy(me->me_key.med_string, $1,
				sizeof(me->me_key.med_string)) >=
			    sizeof(me->me_key.med_string)) {
				yyerror("map element too long: %s", $1);
				free(me);
				free($1);
				YYERROR;
			}
			free($1);

			TAILQ_INSERT_TAIL(&m->m_contents, me, me_entry);
			TAILQ_INSERT_TAIL(conf->sc_maps, m, m_entry);
			$$ = m->m_id;
		}
		| '('				{
			struct map	*m;

			if ((m = calloc(1, sizeof(*m))) == NULL)
				fatal("out of memory");

			m->m_id = last_map_id++;
			if (m->m_id == INT_MAX) {
				yyerror("too many maps defined");
				free(m);
				YYERROR;
			}
			if (! bsnprintf(m->m_name, sizeof(m->m_name),
				"<dynamic(%u)>", m->m_id))
				fatal("snprintf");

			TAILQ_INIT(&m->m_contents);
			contents = &m->m_contents;
			map = m;

		} string_list ')'		{
			TAILQ_INSERT_TAIL(conf->sc_maps, map, m_entry);
			$$ = map->m_id;
		}
		| '{'				{
			struct map	*m;

			if ((m = calloc(1, sizeof(*m))) == NULL)
				fatal("out of memory");

			m->m_id = last_map_id++;
			if (m->m_id == INT_MAX) {
				yyerror("too many maps defined");
				free(m);
				YYERROR;
			}
			if (! bsnprintf(m->m_name, sizeof(m->m_name),
				"<dynamic(%u)>", m->m_id))
				fatal("snprintf");

			TAILQ_INIT(&m->m_contents);
			contents = &m->m_contents;
			map = m;

		} keyval_list '}'		{
			TAILQ_INSERT_TAIL(conf->sc_maps, map, m_entry);
			$$ = map->m_id;
		}
		| MAP STRING			{
			struct map	*m;

			if ((m = map_findbyname($2)) == NULL) {
				yyerror("no such map: %s", $2);
				free($2);
				YYERROR;
			}
			free($2);
			$$ = m->m_id;
		}
		;

decision	: ACCEPT			{ $$ = 1; }
		| REJECT			{ $$ = 0; }
		;

alias		: ALIAS STRING			{ $$ = $2; }
		| /* empty */			{ $$ = NULL; }
		;

condition	: DOMAIN mapref	alias		{
			struct cond	*c;
			struct map	*m;

			if ($3) {
				if ((m = map_findbyname($3)) == NULL) {
					yyerror("no such map: %s", $3);
					free($3);
					YYERROR;
				}
				rule->r_amap = m->m_id;
			}

			if ((c = calloc(1, sizeof *c)) == NULL)
				fatal("out of memory");
			c->c_type = C_DOM;
			c->c_map = $2;
			$$ = c;
		}
		| VIRTUAL mapref		{
			struct cond	*c;
			struct map	*m;

			m = map_find($2);
			if (m->m_src == S_NONE) {
				yyerror("virtual parameter MUST be a map");
				YYERROR;
			}

			if ((c = calloc(1, sizeof *c)) == NULL)
				fatal("out of memory");
			c->c_type = C_VDOM;
			c->c_map = $2;
			$$ = c;
		}
		| LOCAL alias {
			struct cond	*c;
			struct map	*m;
			struct mapel	*me;

			if ($2) {
				if ((m = map_findbyname($2)) == NULL) {
					yyerror("no such map: %s", $2);
					free($2);
					YYERROR;
				}
				rule->r_amap = m->m_id;
			}

			if ((m = calloc(1, sizeof(*m))) == NULL)
				fatal("out of memory");
			m->m_id = last_map_id++;
			if (m->m_id == INT_MAX) {
				yyerror("too many maps defined");
				free(m);
				YYERROR;
			}
			if (! bsnprintf(m->m_name, sizeof(m->m_name),
				"<dynamic(%u)>", m->m_id))
				fatal("snprintf");

			TAILQ_INIT(&m->m_contents);

			if ((me = calloc(1, sizeof(*me))) == NULL)
				fatal("out of memory");

			(void)strlcpy(me->me_key.med_string, "localhost",
			    sizeof(me->me_key.med_string));
			TAILQ_INSERT_TAIL(&m->m_contents, me, me_entry);

			if ((me = calloc(1, sizeof(*me))) == NULL)
				fatal("out of memory");

			if (gethostname(me->me_key.med_string,
				sizeof(me->me_key.med_string)) == -1) {
				yyerror("gethostname() failed");
				free(me);
				free(m);
				YYERROR;
			}
			TAILQ_INSERT_TAIL(&m->m_contents, me, me_entry);

			TAILQ_INSERT_TAIL(conf->sc_maps, m, m_entry);

			if ((c = calloc(1, sizeof *c)) == NULL)
				fatal("out of memory");
			c->c_type = C_DOM;
			c->c_map = m->m_id;

			$$ = c;
		}
		| ALL alias			{
			struct cond	*c;
			struct map	*m;

			if ((c = calloc(1, sizeof *c)) == NULL)
				fatal("out of memory");
			c->c_type = C_ALL;

			if ($2) {
				if ((m = map_findbyname($2)) == NULL) {
					yyerror("no such map: %s", $2);
					free($2);
					YYERROR;
				}
				rule->r_amap = m->m_id;
			}
			$$ = c;
		}
		;

condition_list	: condition comma condition_list	{
			TAILQ_INSERT_TAIL(conditions, $1, c_entry);
		}
		| condition	{
			TAILQ_INSERT_TAIL(conditions, $1, c_entry);
		}
		;

conditions	: condition				{
			TAILQ_INSERT_TAIL(conditions, $1, c_entry);
		}
		| '{' condition_list '}'
		;

user		: AS STRING		{
			struct passwd *pw;

			pw = getpwnam($2);
			if (pw == NULL) {
				yyerror("user '%s' does not exist.", $2);
				free($2);
				YYERROR;
			}
			$$ = $2;
		}
		| /* empty */		{ $$ = NULL; }
		;

relay_as     	: AS STRING		{
			struct mailaddr maddr, *maddrp;
			char *p;

			bzero(&maddr, sizeof (maddr));

			p = strrchr($2, '@');
			if (p == NULL) {
				if (strlcpy(maddr.user, $2, sizeof (maddr.user))
				    >= sizeof (maddr.user))
					yyerror("user-part too long");
					free($2);
					YYERROR;
			}
			else {
				if (p == $2) {
					/* domain only */
					p++;
					if (strlcpy(maddr.domain, p, sizeof (maddr.domain))
					    >= sizeof (maddr.domain)) {
						yyerror("user-part too long");
						free($2);
						YYERROR;
					}
				}
				else {
					*p++ = '\0';
					if (strlcpy(maddr.user, $2, sizeof (maddr.user))
					    >= sizeof (maddr.user)) {
						yyerror("user-part too long");
						free($2);
						YYERROR;
					}
					if (strlcpy(maddr.domain, p, sizeof (maddr.domain))
					    >= sizeof (maddr.domain)) {
						yyerror("domain-part too long");
						free($2);
						YYERROR;
					}
				}
			}

			if (maddr.user[0] == '\0' && maddr.domain[0] == '\0') {
				yyerror("invalid 'relay as' value");
				free($2);
				YYERROR;
			}

			if (maddr.domain[0] == '\0') {
				if (strlcpy(maddr.domain, conf->sc_hostname,
					sizeof (maddr.domain))
				    >= sizeof (maddr.domain)) {
					fatalx("domain too long");
					yyerror("domain-part too long");
					free($2);
					YYERROR;
				}
			}
			
			maddrp = calloc(1, sizeof (*maddrp));
			if (maddrp == NULL)
				fatal("calloc");
			*maddrp = maddr;
			free($2);

			$$ = maddrp;
		}
		| /* empty */		{ $$ = NULL; }
		;

action		: DELIVER TO MAILDIR user		{
			rule->r_user = $4;
			rule->r_action = A_MAILDIR;
			if (strlcpy(rule->r_value.buffer, "~/Maildir",
			    sizeof(rule->r_value.buffer)) >=
			    sizeof(rule->r_value.buffer))
				fatal("pathname too long");
		}
		| DELIVER TO MAILDIR STRING user	{
			rule->r_user = $5;
			rule->r_action = A_MAILDIR;
			if (strlcpy(rule->r_value.buffer, $4,
			    sizeof(rule->r_value.buffer)) >=
			    sizeof(rule->r_value.buffer))
				fatal("pathname too long");
			free($4);
		}
		| DELIVER TO MBOX			{
			rule->r_action = A_MBOX;
			if (strlcpy(rule->r_value.buffer, _PATH_MAILDIR "/%u",
			    sizeof(rule->r_value.buffer))
			    >= sizeof(rule->r_value.buffer))
				fatal("pathname too long");
		}
		| DELIVER TO MDA STRING user		{
			rule->r_user = $5;
			rule->r_action = A_MDA;
			if (strlcpy(rule->r_value.buffer, $4,
			    sizeof(rule->r_value.buffer))
			    >= sizeof(rule->r_value.buffer))
				fatal("command too long");
			free($4);
		}
		| RELAY relay_as     			{
			rule->r_action = A_RELAY;
			rule->r_as = $2;
		}
		| RELAY BACKUP STRING relay_as     		{
			rule->r_action = A_RELAY;
			rule->r_as = $4;
			rule->r_value.relayhost.flags |= F_BACKUP;
			strlcpy(rule->r_value.relayhost.hostname, $3,
			    sizeof (rule->r_value.relayhost.hostname));
			free($3);
		}
		| RELAY VIA STRING certname credentials relay_as {
			rule->r_action = A_RELAYVIA;
			rule->r_as = $6;

			if (! text_to_relayhost(&rule->r_value.relayhost, $3)) {
				yyerror("error: invalid url: %s", $3);
				free($3);
				free($4);
				free($5);
				free($6);
				YYERROR;
			}
			free($3);

			/* no worries, F_AUTH cant be set without SSL */
			if (rule->r_value.relayhost.flags & F_AUTH) {
				if ($5 == NULL) {
					yyerror("error: auth without authmap");
					free($3);
					free($4);
					free($5);
					free($6);
					YYERROR;
				}
				strlcpy(rule->r_value.relayhost.authmap, $5,
				    sizeof(rule->r_value.relayhost.authmap));
			}
			free($5);


			if ($4 != NULL) {
				if (ssl_load_certfile($4, F_CCERT) < 0) {
					yyerror("cannot load certificate: %s",
					    $4);
					free($3);
					free($4);
					free($5);
					free($6);
					YYERROR;
				}
				if (strlcpy(rule->r_value.relayhost.cert, $4,
					sizeof(rule->r_value.relayhost.cert))
				    >= sizeof(rule->r_value.relayhost.cert))
					fatal("certificate path too long");
			}
			free($4);
		}
		;

from		: FROM mapref			{
			$$ = $2;
		}
		| FROM ALL			{
			struct map	*m;
			struct mapel	*me;

			if ((m = calloc(1, sizeof(*m))) == NULL)
				fatal("out of memory");
			m->m_id = last_map_id++;
			if (m->m_id == INT_MAX) {
				yyerror("too many maps defined");
				free(m);
				YYERROR;
			}
			if (! bsnprintf(m->m_name, sizeof(m->m_name),
				"<dynamic(%u)>", m->m_id))
				fatal("snprintf");

			TAILQ_INIT(&m->m_contents);

			if ((me = calloc(1, sizeof(*me))) == NULL)
				fatal("out of memory");
			(void)strlcpy(me->me_key.med_string, "0.0.0.0/0",
			    sizeof(me->me_key.med_string));
			TAILQ_INSERT_TAIL(&m->m_contents, me, me_entry);

			if ((me = calloc(1, sizeof(*me))) == NULL)
				fatal("out of memory");
			(void)strlcpy(me->me_key.med_string, "::/0",
			    sizeof(me->me_key.med_string));
			TAILQ_INSERT_TAIL(&m->m_contents, me, me_entry);

			TAILQ_INSERT_TAIL(conf->sc_maps, m, m_entry);
			$$ = m->m_id;
		}
		| FROM LOCAL			{
			struct map	*m;

			m = map_findbyname("localhost");
			$$ = m->m_id;
		}
		| /* empty */			{
			struct map	*m;

			m = map_findbyname("localhost");
			$$ = m->m_id;
		}
		;

on		: ON STRING	{
       			if (strlen($2) >= MAX_TAG_SIZE) {
       				yyerror("interface, address or tag name too long");
				free($2);
				YYERROR;
			}

			$$ = $2;
		}
		| /* empty */	{ $$ = NULL; }
		;

rule		: decision on from			{

			if ((rule = calloc(1, sizeof(*rule))) == NULL)
				fatal("out of memory");
			rule->r_sources = map_find($3);


			if ((conditions = calloc(1, sizeof(*conditions))) == NULL)
				fatal("out of memory");

			if ($2)
				(void)strlcpy(rule->r_tag, $2, sizeof(rule->r_tag));
			free($2);


			TAILQ_INIT(conditions);

		} FOR conditions action	tag expire {
			struct rule	*subr;
			struct cond	*cond;

			if ($8)
				(void)strlcpy(rule->r_tag, $8, sizeof(rule->r_tag));
			free($8);

			rule->r_qexpire = $9;

			while ((cond = TAILQ_FIRST(conditions)) != NULL) {

				if ((subr = calloc(1, sizeof(*subr))) == NULL)
					fatal("out of memory");

				*subr = *rule;

				subr->r_condition = *cond;
				
				TAILQ_REMOVE(conditions, cond, c_entry);
				TAILQ_INSERT_TAIL(conf->sc_rules, subr, r_entry);

				free(cond);
			}

			if (rule->r_amap) {
				if (rule->r_action == A_RELAY ||
				    rule->r_action == A_RELAYVIA) {
					yyerror("aliases set on a relay rule");
					free(conditions);
					free(rule);
					YYERROR;
				}
			}

			free(conditions);
			free(rule);
			conditions = NULL;
			rule = NULL;
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

	file->errors++;
	va_start(ap, fmt);
	fprintf(stderr, "%s:%d: ", file->name, yylval.lineno);
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
		{ "accept",		ACCEPT },
		{ "alias",		ALIAS },
		{ "all",		ALL },
		{ "as",			AS },
		{ "auth",		AUTH },
		{ "backup",		BACKUP },
		{ "certificate",	CERTIFICATE },
		{ "compress",		COMPRESS },
		{ "db",			DB },
		{ "deliver",		DELIVER },
		{ "domain",		DOMAIN },
		{ "enable",		ENABLE },
		{ "expire",		EXPIRE },
		{ "filter",		FILTER },
		{ "for",		FOR },
		{ "from",		FROM },
		{ "hash",		HASH },
		{ "hostname",		HOSTNAME },
		{ "include",		INCLUDE },
		{ "interval",		INTERVAL },
		{ "list",		LIST },
		{ "listen",		LISTEN },
		{ "local",		LOCAL },
		{ "maildir",		MAILDIR },
		{ "map",		MAP },
		{ "mbox",		MBOX },
		{ "mda",		MDA },
		{ "on",			ON },
		{ "plain",		PLAIN },
		{ "port",		PORT },
		{ "queue",		QUEUE },
		{ "reject",		REJECT },
		{ "relay",		RELAY },
		{ "single",		SINGLE },
		{ "size",		SIZE },
		{ "smtps",		SMTPS },
		{ "source",		SOURCE },
		{ "ssl",		SSL },
		{ "tag",		TAG },
		{ "tls",		TLS },
		{ "to",			TO },
		{ "via",		VIA },
		{ "virtual",		VIRTUAL },
	};
	const struct keywords	*p;

	p = bsearch(s, keywords, sizeof(keywords)/sizeof(keywords[0]),
	    sizeof(keywords[0]), kw_cmp);

	if (p)
		return (p->k_val);
	else
		return (STRING);
}

#define MAXPUSHBACK	128

char	*parsebuf;
int	 parseindex;
char	 pushback_buffer[MAXPUSHBACK];
int	 pushback_index = 0;

int
lgetc(int quotec)
{
	int		c, next;

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

	if (quotec) {
		if ((c = getc(file->stream)) == EOF) {
			yyerror("reached end of file while parsing "
			    "quoted string");
			if (file == topfile || popfile() == EOF)
				return (EOF);
			return (quotec);
		}
		return (c);
	}

	while ((c = getc(file->stream)) == '\\') {
		next = getc(file->stream);
		if (next != '\n') {
			c = next;
			break;
		}
		yylval.lineno = file->lineno;
		file->lineno++;
	}

	while (c == EOF) {
		if (file == topfile || popfile() == EOF)
			return (EOF);
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
		c = lgetc(0);
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
	int	 quotec, next, c;
	int	 token;

top:
	p = buf;
	while ((c = lgetc(0)) == ' ' || c == '\t')
		; /* nothing */

	yylval.lineno = file->lineno;
	if (c == '#')
		while ((c = lgetc(0)) != '\n' && c != EOF)
			; /* nothing */
	if (c == '$' && parsebuf == NULL) {
		while (1) {
			if ((c = lgetc(0)) == EOF)
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
		quotec = c;
		while (1) {
			if ((c = lgetc(quotec)) == EOF)
				return (0);
			if (c == '\n') {
				file->lineno++;
				continue;
			} else if (c == '\\') {
				if ((next = lgetc(quotec)) == EOF)
					return (0);
				if (next == quotec || c == ' ' || c == '\t')
					c = next;
				else if (next == '\n') {
					file->lineno++;
					continue;
				} else
					lungetc(next);
			} else if (c == quotec) {
				*p = '\0';
				break;
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

#define allowed_to_end_number(x) \
	(isspace(x) || x == ')' || x ==',' || x == '/' || x == '}' || x == '=')

	if (c == '-' || isdigit(c)) {
		do {
			*p++ = c;
			if ((unsigned)(p-buf) >= sizeof(buf)) {
				yyerror("string too long");
				return (findeol());
			}
		} while ((c = lgetc(0)) != EOF && isdigit(c));
		lungetc(c);
		if (p == buf + 1 && buf[0] == '-')
			goto nodigits;
		if (c == EOF || allowed_to_end_number(c)) {
			const char *errstr = NULL;

			*p = '\0';
			yylval.v.number = strtonum(buf, LLONG_MIN,
			    LLONG_MAX, &errstr);
			if (errstr) {
				yyerror("\"%s\" invalid number: %s",
				    buf, errstr);
				return (findeol());
			}
			return (NUMBER);
		} else {
nodigits:
			while (p > buf + 1)
				lungetc(*--p);
			c = *--p;
			if (c == '-')
				return (c);
		}
	}

	if (c == '=') {
		if ((c = lgetc(0)) != EOF && c == '>')
			return (ARROW);
		lungetc(c);
		c = '=';
	}

#define allowed_in_string(x) \
	(isalnum(x) || (ispunct(x) && x != '(' && x != ')' && \
	x != '{' && x != '}' && x != '<' && x != '>' && \
	x != '!' && x != '=' && x != '#' && \
	x != ','))

	if (isalnum(c) || c == ':' || c == '_') {
		do {
			*p++ = c;
			if ((unsigned)(p-buf) >= sizeof(buf)) {
				yyerror("string too long");
				return (findeol());
			}
		} while ((c = lgetc(0)) != EOF && (allowed_in_string(c)));
		lungetc(c);
		*p = '\0';
		if ((token = lookup(buf)) == STRING)
			if ((yylval.v.string = strdup(buf)) == NULL)
				err(1, "yylex: strdup");
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
check_file_secrecy(int fd, const char *fname)
{
	struct stat	st;

	if (fstat(fd, &st)) {
		log_warn("cannot stat %s", fname);
		return (-1);
	}
	if (st.st_uid != 0 && st.st_uid != getuid()) {
		log_warnx("%s: owner not root or current user", fname);
		return (-1);
	}
	if (st.st_mode & (S_IRWXG | S_IRWXO)) {
		log_warnx("%s: group/world readable/writeable", fname);
		return (-1);
	}
	return (0);
}

struct file *
pushfile(const char *name, int secret)
{
	struct file	*nfile;

	if ((nfile = calloc(1, sizeof(struct file))) == NULL) {
		log_warn("malloc");
		return (NULL);
	}
	if ((nfile->name = strdup(name)) == NULL) {
		log_warn("malloc");
		free(nfile);
		return (NULL);
	}
	if ((nfile->stream = fopen(nfile->name, "r")) == NULL) {
		log_warn("%s", nfile->name);
		free(nfile->name);
		free(nfile);
		return (NULL);
	} else if (secret &&
	    check_file_secrecy(fileno(nfile->stream), nfile->name)) {
		fclose(nfile->stream);
		free(nfile->name);
		free(nfile);
		return (NULL);
	}
	nfile->lineno = 1;
	TAILQ_INSERT_TAIL(&files, nfile, entry);
	return (nfile);
}

int
popfile(void)
{
	struct file	*prev;

	if ((prev = TAILQ_PREV(file, files, entry)) != NULL)
		prev->errors += file->errors;

	TAILQ_REMOVE(&files, file, entry);
	fclose(file->stream);
	free(file->name);
	free(file);
	file = prev;
	return (file ? 0 : EOF);
}

int
parse_config(struct smtpd *x_conf, const char *filename, int opts)
{
	struct sym	*sym, *next;
	struct map	*m;

	conf = x_conf;
	bzero(conf, sizeof(*conf));

	conf->sc_maxsize = SIZE_MAX;

	conf->sc_maps = calloc(1, sizeof(*conf->sc_maps));
	conf->sc_rules = calloc(1, sizeof(*conf->sc_rules));
	conf->sc_listeners = calloc(1, sizeof(*conf->sc_listeners));
	conf->sc_ssl = calloc(1, sizeof(*conf->sc_ssl));
	conf->sc_filters = calloc(1, sizeof(*conf->sc_filters));
	m = calloc(1, sizeof(*m));

	if (conf->sc_maps == NULL	||
	    conf->sc_rules == NULL	||
	    conf->sc_listeners == NULL	||
	    conf->sc_ssl == NULL	||
	    conf->sc_filters == NULL	||
	    m == NULL) {
		log_warn("cannot allocate memory");
		free(conf->sc_maps);
		free(conf->sc_rules);
		free(conf->sc_listeners);
		free(conf->sc_ssl);
		free(conf->sc_filters);
		free(m);
		return (-1);
	}

	errors = 0;
	last_map_id = 1;

	map = NULL;
	rule = NULL;

	TAILQ_INIT(conf->sc_listeners);
	TAILQ_INIT(conf->sc_maps);
	TAILQ_INIT(conf->sc_rules);
	TAILQ_INIT(conf->sc_filters);
	SPLAY_INIT(conf->sc_ssl);
	SPLAY_INIT(&conf->sc_sessions);

	conf->sc_qexpire = SMTPD_QUEUE_EXPIRY;
	conf->sc_qintval.tv_sec = SMTPD_QUEUE_INTERVAL;
	conf->sc_qintval.tv_usec = 0;
	conf->sc_opts = opts;

	if ((file = pushfile(filename, 0)) == NULL) {
		purge_config(PURGE_EVERYTHING);
		free(m);
		return (-1);
	}
	topfile = file;

	/*
	 * declare special "local" map
	 */
	m->m_id = last_map_id++;
	if (strlcpy(m->m_name, "localhost", sizeof(m->m_name))
	    >= sizeof(m->m_name))
		fatal("strlcpy");
	TAILQ_INIT(&m->m_contents);
	TAILQ_INSERT_TAIL(conf->sc_maps, m, m_entry);
	set_localaddrs();

	/*
	 * parse configuration
	 */
	setservent(1);
	yyparse();
	errors = file->errors;
	popfile();
	endservent();

	/* Free macros and check which have not been used. */
	for (sym = TAILQ_FIRST(&symhead); sym != NULL; sym = next) {
		next = TAILQ_NEXT(sym, entry);
		if ((conf->sc_opts & SMTPD_OPT_VERBOSE) && !sym->used)
			fprintf(stderr, "warning: macro '%s' not "
			    "used\n", sym->nam);
		if (!sym->persist) {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entry);
			free(sym);
		}
	}

	if (TAILQ_EMPTY(conf->sc_rules)) {
		log_warnx("no rules, nothing to do");
		errors++;
	}

	if (strlen(conf->sc_hostname) == 0)
		if (gethostname(conf->sc_hostname,
		    sizeof(conf->sc_hostname)) == -1) {
			log_warn("could not determine host name");
			bzero(conf->sc_hostname, sizeof(conf->sc_hostname));
			errors++;
		}

	if (errors) {
		purge_config(PURGE_EVERYTHING);
		return (-1);
	}

	return (0);
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
	TAILQ_INSERT_TAIL(&symhead, sym, entry);
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
		errx(1, "cmdline_symset: malloc");

	(void)strlcpy(sym, s, len);

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

struct listener *
host_v4(const char *s, in_port_t port)
{
	struct in_addr		 ina;
	struct sockaddr_in	*sain;
	struct listener		*h;

	bzero(&ina, sizeof(ina));
	if (inet_pton(AF_INET, s, &ina) != 1)
		return (NULL);

	if ((h = calloc(1, sizeof(*h))) == NULL)
		fatal(NULL);
	sain = (struct sockaddr_in *)&h->ss;
	sain->sin_len = sizeof(struct sockaddr_in);
	sain->sin_family = AF_INET;
	sain->sin_addr.s_addr = ina.s_addr;
	sain->sin_port = port;

	return (h);
}

struct listener *
host_v6(const char *s, in_port_t port)
{
	struct in6_addr		 ina6;
	struct sockaddr_in6	*sin6;
	struct listener		*h;

	bzero(&ina6, sizeof(ina6));
	if (inet_pton(AF_INET6, s, &ina6) != 1)
		return (NULL);

	if ((h = calloc(1, sizeof(*h))) == NULL)
		fatal(NULL);
	sin6 = (struct sockaddr_in6 *)&h->ss;
	sin6->sin6_len = sizeof(struct sockaddr_in6);
	sin6->sin6_family = AF_INET6;
	sin6->sin6_port = port;
	memcpy(&sin6->sin6_addr, &ina6, sizeof(ina6));

	return (h);
}

int
host_dns(const char *s, const char *tag, const char *cert,
    struct listenerlist *al, int max, in_port_t port, uint8_t flags)
{
	struct addrinfo		 hints, *res0, *res;
	int			 error, cnt = 0;
	struct sockaddr_in	*sain;
	struct sockaddr_in6	*sin6;
	struct listener		*h;

	bzero(&hints, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM; /* DUMMY */
	error = getaddrinfo(s, NULL, &hints, &res0);
	if (error == EAI_AGAIN || error == EAI_NODATA || error == EAI_NONAME)
		return (0);
	if (error) {
		log_warnx("host_dns: could not parse \"%s\": %s", s,
		    gai_strerror(error));
		return (-1);
	}

	for (res = res0; res && cnt < max; res = res->ai_next) {
		if (res->ai_family != AF_INET &&
		    res->ai_family != AF_INET6)
			continue;
		if ((h = calloc(1, sizeof(*h))) == NULL)
			fatal(NULL);

		h->port = port;
		h->flags = flags;
		h->ss.ss_family = res->ai_family;
		h->ssl = NULL;
		h->ssl_cert_name[0] = '\0';
		if (cert != NULL)
			(void)strlcpy(h->ssl_cert_name, cert, sizeof(h->ssl_cert_name));
		if (tag != NULL)
			(void)strlcpy(h->tag, tag, sizeof(h->tag));

		if (res->ai_family == AF_INET) {
			sain = (struct sockaddr_in *)&h->ss;
			sain->sin_len = sizeof(struct sockaddr_in);
			sain->sin_addr.s_addr = ((struct sockaddr_in *)
			    res->ai_addr)->sin_addr.s_addr;
			sain->sin_port = port;
		} else {
			sin6 = (struct sockaddr_in6 *)&h->ss;
			sin6->sin6_len = sizeof(struct sockaddr_in6);
			memcpy(&sin6->sin6_addr, &((struct sockaddr_in6 *)
			    res->ai_addr)->sin6_addr, sizeof(struct in6_addr));
			sin6->sin6_port = port;
		}

		TAILQ_INSERT_HEAD(al, h, entry);
		cnt++;
	}
	if (cnt == max && res) {
		log_warnx("host_dns: %s resolves to more than %d hosts",
		    s, max);
	}
	freeaddrinfo(res0);
	return (cnt);
}

int
host(const char *s, const char *tag, const char *cert, struct listenerlist *al,
    int max, in_port_t port, uint8_t flags)
{
	struct listener *h;

	h = host_v4(s, port);

	/* IPv6 address? */
	if (h == NULL)
		h = host_v6(s, port);

	if (h != NULL) {
		h->port = port;
		h->flags = flags;
		h->ssl = NULL;
		h->ssl_cert_name[0] = '\0';
		if (cert != NULL)
			(void)strlcpy(h->ssl_cert_name, cert, sizeof(h->ssl_cert_name));
		if (tag != NULL)
			(void)strlcpy(h->tag, tag, sizeof(h->tag));

		TAILQ_INSERT_HEAD(al, h, entry);
		return (1);
	}

	return (host_dns(s, tag, cert, al, max, port, flags));
}

int
interface(const char *s, const char *tag, const char *cert,
    struct listenerlist *al, int max, in_port_t port, uint8_t flags)
{
	struct ifaddrs *ifap, *p;
	struct sockaddr_in	*sain;
	struct sockaddr_in6	*sin6;
	struct listener		*h;
	int ret = 0;

	if (getifaddrs(&ifap) == -1)
		fatal("getifaddrs");

	for (p = ifap; p != NULL; p = p->ifa_next) {
		if (p->ifa_addr == NULL)
			continue;
		if (strcmp(p->ifa_name, s) != 0 &&
		    ! is_if_in_group(p->ifa_name, s))
			continue;

		if ((h = calloc(1, sizeof(*h))) == NULL)
			fatal(NULL);

		switch (p->ifa_addr->sa_family) {
		case AF_INET:
			sain = (struct sockaddr_in *)&h->ss;
			*sain = *(struct sockaddr_in *)p->ifa_addr;
			sain->sin_len = sizeof(struct sockaddr_in);
			sain->sin_port = port;
			break;

		case AF_INET6:
			sin6 = (struct sockaddr_in6 *)&h->ss;
			*sin6 = *(struct sockaddr_in6 *)p->ifa_addr;
			sin6->sin6_len = sizeof(struct sockaddr_in6);
			sin6->sin6_port = port;
			break;

		default:
			free(h);
			continue;
		}

		h->fd = -1;
		h->port = port;
		h->flags = flags;
		h->ssl = NULL;
		h->ssl_cert_name[0] = '\0';
		if (cert != NULL)
			(void)strlcpy(h->ssl_cert_name, cert, sizeof(h->ssl_cert_name));
		if (tag != NULL)
			(void)strlcpy(h->tag, tag, sizeof(h->tag));

		ret = 1;
		TAILQ_INSERT_HEAD(al, h, entry);
	}

	freeifaddrs(ifap);

	return ret;
}

void
set_localaddrs(void)
{
	struct ifaddrs *ifap, *p;
	struct sockaddr_storage ss;
	struct sockaddr_in	*sain;
	struct sockaddr_in6	*sin6;
	struct map		*m;
	struct mapel		*me;

	if (getifaddrs(&ifap) == -1)
		fatal("getifaddrs");

	m = map_findbyname("localhost");

	for (p = ifap; p != NULL; p = p->ifa_next) {
		if (p->ifa_addr == NULL)
			continue;
		switch (p->ifa_addr->sa_family) {
		case AF_INET:
			sain = (struct sockaddr_in *)&ss;
			*sain = *(struct sockaddr_in *)p->ifa_addr;
			sain->sin_len = sizeof(struct sockaddr_in);
			if ((me = calloc(1, sizeof(*me))) == NULL)
				fatal("out of memory");
			(void)strlcpy(me->me_key.med_string,
			    ss_to_text(&ss),
			    sizeof(me->me_key.med_string));
			TAILQ_INSERT_TAIL(&m->m_contents, me, me_entry);
			break;

		case AF_INET6:
			sin6 = (struct sockaddr_in6 *)&ss;
			*sin6 = *(struct sockaddr_in6 *)p->ifa_addr;
			sin6->sin6_len = sizeof(struct sockaddr_in6);
			if ((me = calloc(1, sizeof(*me))) == NULL)
				fatal("out of memory");
			(void)strlcpy(me->me_key.med_string,
			    ss_to_text(&ss),
			    sizeof(me->me_key.med_string));
			TAILQ_INSERT_TAIL(&m->m_contents, me, me_entry);
			break;
		}
	}

	freeifaddrs(ifap);
}

int
delaytonum(char *str)
{
	unsigned int     factor;
	size_t           len;
	const char      *errstr = NULL;
	int              delay;
  	
	/* we need at least 1 digit and 1 unit */
	len = strlen(str);
	if (len < 2)
		goto bad;
	
	switch(str[len - 1]) {
		
	case 's':
		factor = 1;
		break;
		
	case 'm':
		factor = 60;
		break;
		
	case 'h':
		factor = 60 * 60;
		break;
		
	case 'd':
		factor = 24 * 60 * 60;
		break;
		
	default:
		goto bad;
	}
  	
	str[len - 1] = '\0';
	delay = strtonum(str, 1, INT_MAX / factor, &errstr);
	if (errstr)
		goto bad;
	
	return (delay * factor);
  	
bad:
	return (-1);
}

int
is_if_in_group(const char *ifname, const char *groupname)
{
        unsigned int		 len;
        struct ifgroupreq        ifgr;
        struct ifg_req          *ifg;
	int			 s;
	int			 ret = 0;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		err(1, "socket");

        memset(&ifgr, 0, sizeof(ifgr));
        strlcpy(ifgr.ifgr_name, ifname, IFNAMSIZ);
        if (ioctl(s, SIOCGIFGROUP, (caddr_t)&ifgr) == -1) {
                if (errno == EINVAL || errno == ENOTTY)
			goto end;
		err(1, "SIOCGIFGROUP");
        }

        len = ifgr.ifgr_len;
        ifgr.ifgr_groups =
            (struct ifg_req *)calloc(len/sizeof(struct ifg_req),
		sizeof(struct ifg_req));
        if (ifgr.ifgr_groups == NULL)
                err(1, "getifgroups");
        if (ioctl(s, SIOCGIFGROUP, (caddr_t)&ifgr) == -1)
                err(1, "SIOCGIFGROUP");
	
        ifg = ifgr.ifgr_groups;
        for (; ifg && len >= sizeof(struct ifg_req); ifg++) {
                len -= sizeof(struct ifg_req);
		if (strcmp(ifg->ifgrq_group, groupname) == 0) {
			ret = 1;
			break;
		}
        }
        free(ifgr.ifgr_groups);

end:
	close(s);
	return ret;
}
