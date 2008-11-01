/*
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
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
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <limits.h>
#include <pwd.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
	
#include "smtpd.h"

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
int		 yyerror(const char *, ...);
int		 kw_cmp(const void *, const void *);
int		 lookup(char *);
int		 lgetc(int);
int		 lungetc(int);
int		 findeol(void);

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

objid_t			 last_map_id = 0;
struct map		*map = NULL;
struct rule		*rule = NULL;
struct mapel_list	*contents = NULL;

struct listener	*host_v4(const char *, in_port_t);
struct listener	*host_v6(const char *, in_port_t);
int		 host_dns(const char *, struct listenerlist *,
		    int, in_port_t, u_int8_t);
int		 host(const char *, struct listenerlist *,
		    int, in_port_t, u_int8_t);

typedef struct {
	union {
		int64_t		 number;
		objid_t		 object;
		struct timeval	 tv;
		struct cond	*cond;
		char		*string;
		struct host	*host;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	QUEUE INTERVAL LISTEN ON ALL PORT USE
%token	MAP TYPE HASH LIST SINGLE SSL SSMTP CERTIFICATE
%token	DNS DB TFILE EXTERNAL DOMAIN CONFIG SOURCE
%token  RELAY VIA DELIVER TO MAILDIR MBOX HOSTNAME
%token	ACCEPT REJECT INCLUDE NETWORK ERROR MDA FROM FOR
%token	<v.string>	STRING
%token  <v.number>	NUMBER
%type	<v.map>		map
%type	<v.number>	quantifier decision port ssmtp from
%type	<v.cond>	condition
%type	<v.tv>		interval
%type	<v.object>	mapref
%type	<v.string>	certname

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

nl		: '\n' optnl
		;

quantifier	: /* empty */			{ $$ = 1; }
		| 'm'				{ $$ = 60; }
		| 'h'				{ $$ = 3600; }
		| 'd'				{ $$ = 86400; }
		;

interval	: NUMBER quantifier		{
			if ($1 < 0) {
				yyerror("invalid interval: %d\n", $1);
				YYERROR;
			}
			$$.tv_usec = 0;
			$$.tv_sec = $1 * $2;
		}
		

port		: PORT STRING 			{
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
		| PORT NUMBER 			{
			if ($2 <= 0 || $2 >= (int)USHRT_MAX) {
				yyerror("invalid port: %d", $2);
				YYERROR;
			}
			$$ = htons($2);
		}
		| /* empty */			{
			$$ = 0;
		}
		;

certname	: USE CERTIFICATE STRING	{
			if (($$ = strdup($3)) == NULL)
				fatal(NULL);
			free($3);
		}
		| /* empty */			{ $$ = NULL; }
		;

ssmtp		: SSMTP				{ $$ = 1; }
		| /* empty */			{ $$ = 0; }
		;

main		: QUEUE INTERVAL interval 	{
			conf->sc_qintval = $3;
		}
		| ssmtp LISTEN ON STRING port certname {
			char		*cert;
			u_int8_t	 flags;

			if ($5 == 0) {
				if ($1)
					$5 = htons(487);
				else
					$5 = htons(25);
			}
			cert = ($6 != NULL) ? $6 : $4;

			flags = 0;
			if (ssl_load_certfile(conf, cert) < 0) {
				log_warnx("could not load cert: %s", cert);
				if ($1 || $6 != NULL) {
					yyerror("cannot load certificate: %s",
					    cert);
					free($6);
					free($4);
					YYERROR;
				}
			}
			else {
				if ($1)
					flags = F_SSMTP;
				else
					flags = F_STARTTLS;
			}

			if (host($4, &conf->sc_listeners,
			    MAX_LISTEN, $5, flags) <= 0) {
				yyerror("invalid virtual ip: %s", $4);  
				free($6);
				free($4);
				YYERROR;
			}
			free($6);
			free($4);
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
		}
		;

maptype		: SINGLE			{ map->m_type = T_SINGLE; }
		| LIST				{ map->m_type = T_LIST; }
		| HASH				{ map->m_type = T_HASH; }
		;

mapsource	: DNS				{ map->m_src = S_DNS; }
		| TFILE				{ map->m_src = S_FILE; }
		| DB STRING    			{
			map->m_src = S_DB;
			if (strlcpy(map->m_config, $2, MAXPATHLEN)
			    >= MAXPATHLEN)
				err(1, "pathname too long");
		}
		| EXTERNAL			{ map->m_src = S_EXT; }
		;

mapopt		: TYPE maptype
		| SOURCE mapsource
		| CONFIG STRING			{
		}
		;

mapopts_l	: mapopts_l mapopt nl
		| mapopt optnl
		;

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
			m->m_type = T_SINGLE;

			if (m->m_id == INT_MAX) {
				yyerror("too many maps defined");
				free($2);
				free(m);
				YYERROR;
			}
			map = m;
		} '{' optnl mapopts_l '}'	{
			if (map->m_src == S_NONE) {
				yyerror("map %s has no source defined");
				free(map);
				map = NULL;
				YYERROR;
			}
			TAILQ_INSERT_TAIL(conf->sc_maps, map, m_entry);
			map = NULL;
		}
		;

keyval		: STRING '=' '>' STRING		{
			struct mapel	*me;

			if ((me = calloc(1, sizeof(*me))) == NULL)
				fatal("out of memory");
			
			if (strlcpy(me->me_key.med_string, $1,
			    sizeof(me->me_key.med_string)) >=
			    sizeof(me->me_key.med_string) ||
			    strlcpy(me->me_val.med_string, $4,
			    sizeof(me->me_val.med_string)) >=
			    sizeof(me->me_val.med_string)) {
				yyerror("map elements too long: %s, %s",
				    $1, $4);
				free(me);
				free($1);
				free($4);
				YYERROR;
			}
			free($1);
			free($4);

			TAILQ_INSERT_TAIL(contents, me, me_entry);
		}

keyval_list	: keyval
		| keyval comma keyval_list
		;

stringel	: STRING			{
			struct mapel	*me;
			int bits;
			struct sockaddr_in ssin;
			struct sockaddr_in6 ssin6;

			if ((me = calloc(1, sizeof(*me))) == NULL)
				fatal("out of memory");

			/* Attempt detection of $1 format */
			if (strchr($1, '/') != NULL) {
				/* Dealing with a netmask */
				bzero(&ssin, sizeof(struct sockaddr_in));
				bits = inet_net_pton(AF_INET, $1, &ssin.sin_addr, sizeof(struct in_addr));
				if (bits != -1) {
					ssin.sin_family = AF_INET;
					me->me_key.med_addr.masked = bits;
					me->me_key.med_addr.ss = *(struct sockaddr_storage *)&ssin;
				}
				else {
					bzero(&ssin6, sizeof(struct sockaddr_in6));
					bits = inet_net_pton(AF_INET6, $1, &ssin6.sin6_addr, sizeof(struct in6_addr));
					if (bits == -1)
						err(1, "inet_net_pton");
					ssin6.sin6_family = AF_INET6;
					me->me_key.med_addr.masked = bits;
					me->me_key.med_addr.ss = *(struct sockaddr_storage *)&ssin6;
				}
			}
			else {
				/* IP address ? */
				if (inet_pton(AF_INET, $1, &ssin.sin_addr) == 1) {
					ssin.sin_family = AF_INET;
					me->me_key.med_addr.masked = 0;
					me->me_key.med_addr.ss = *(struct sockaddr_storage *)&ssin;
				}
				else if (inet_pton(AF_INET6, $1, &ssin6.sin6_addr) == 1) {
					ssin6.sin6_family = AF_INET6;
					me->me_key.med_addr.masked = 0;
					me->me_key.med_addr.ss = *(struct sockaddr_storage *)&ssin6;
				}
				else {
					/* either a hostname or a value unrelated to network */
					if (strlcpy(me->me_key.med_string, $1,
						sizeof(me->me_key.med_string)) >=
					    sizeof(me->me_key.med_string)) {
						yyerror("map element too long: %s", $1);
						free(me);
						free($1);
						YYERROR;
					}
				}
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
			int bits;
			struct sockaddr_in ssin;
			struct sockaddr_in6 ssin6;

			if ((m = calloc(1, sizeof(*m))) == NULL)
				fatal("out of memory");
			m->m_id = last_map_id++;
			if (m->m_id == INT_MAX) {
				yyerror("too many maps defined");
				free(m);
				YYERROR;
			}
			snprintf(m->m_name, STRLEN, "<dynamic(%u)>", m->m_id);
			m->m_flags |= F_DYNAMIC|F_USED;
			m->m_type = T_SINGLE;

			TAILQ_INIT(&m->m_contents);

			if ((me = calloc(1, sizeof(*me))) == NULL)
				fatal("out of memory");

			/* Attempt detection of $1 format */
			if (strchr($1, '/') != NULL) {
				/* Dealing with a netmask */
				bzero(&ssin, sizeof(struct sockaddr_in));
				bits = inet_net_pton(AF_INET, $1, &ssin.sin_addr, sizeof(struct in_addr));
				if (bits != -1) {
					ssin.sin_family = AF_INET;
					me->me_key.med_addr.masked = bits;
					me->me_key.med_addr.ss = *(struct sockaddr_storage *)&ssin;
				}
				else {
					bzero(&ssin6, sizeof(struct sockaddr_in6));
					bits = inet_net_pton(AF_INET6, $1, &ssin6.sin6_addr, sizeof(struct in6_addr));
					if (bits == -1)
						err(1, "inet_net_pton");
					ssin6.sin6_family = AF_INET6;
					me->me_key.med_addr.masked = bits;
					me->me_key.med_addr.ss = *(struct sockaddr_storage *)&ssin6;
				}
			}
			else {
				/* IP address ? */
				if (inet_pton(AF_INET, $1, &ssin.sin_addr) == 1) {
					ssin.sin_family = AF_INET;
					me->me_key.med_addr.masked = 0;
					me->me_key.med_addr.ss = *(struct sockaddr_storage *)&ssin;
				}
				else if (inet_pton(AF_INET6, $1, &ssin6.sin6_addr) == 1) {
					ssin6.sin6_family = AF_INET6;
					me->me_key.med_addr.masked = 0;
					me->me_key.med_addr.ss = *(struct sockaddr_storage *)&ssin6;
				}
				else {
					/* either a hostname or a value unrelated to network */
					if (strlcpy(me->me_key.med_string, $1,
						sizeof(me->me_key.med_string)) >=
					    sizeof(me->me_key.med_string)) {
						yyerror("map element too long: %s", $1);
						free(me);
						free(m);
						free($1);
						YYERROR;
					}
				}
			}
			free($1);

			TAILQ_INSERT_TAIL(&m->m_contents, me, me_entry);
			TAILQ_INSERT_TAIL(conf->sc_maps, m, m_entry);
			$$ = m->m_id;
		}
		| '(' 				{
			struct map	*m;

			if ((m = calloc(1, sizeof(*m))) == NULL)
				fatal("out of memory");

			m->m_id = last_map_id++;
			if (m->m_id == INT_MAX) {
				yyerror("too many maps defined");
				free(m);
				YYERROR;
			}
			snprintf(m->m_name, STRLEN, "<dynamic(%u)>", m->m_id);
			m->m_flags |= F_DYNAMIC|F_USED;
			m->m_type = T_LIST;

			TAILQ_INIT(&m->m_contents);
			contents = &m->m_contents;
			map = m;

		} string_list ')'		{
			TAILQ_INSERT_TAIL(conf->sc_maps, map, m_entry);
			$$ = map->m_id;
		}
		| '{' 				{
			struct map	*m;

			if ((m = calloc(1, sizeof(*m))) == NULL)
				fatal("out of memory");

			m->m_id = last_map_id++;
			if (m->m_id == INT_MAX) {
				yyerror("too many maps defined");
				free(m);
				YYERROR;
			}
			snprintf(m->m_name, STRLEN, "<dynamic(%u)>", m->m_id);
			m->m_flags |= F_DYNAMIC|F_USED;
			m->m_type = T_HASH;

			TAILQ_INIT(&m->m_contents);
			contents = &m->m_contents;
			map = m;

		} keyval_list '}'		{
			TAILQ_INSERT_TAIL(conf->sc_maps, map, m_entry);
			$$ = map->m_id;
		}
		| MAP STRING			{
			struct map	*m;

			if ((m = map_findbyname(conf, $2)) == NULL) {
				yyerror("no such map: %s");
				free($2);
				YYERROR;
			}
			free($2);
			m->m_flags |= F_USED;
			$$ = m->m_id;
		}
		;

decision	: ACCEPT			{ $$ = 1; }
		| REJECT			{ $$ = 0; }
		;

condition	: NETWORK mapref		{
			struct cond	*c;

			if ((c = calloc(1, sizeof *c)) == NULL)
				fatal("out of memory");
			c->c_type = C_NET;
			c->c_map = $2;
			$$ = c;
		}
		| DOMAIN mapref			{
			struct cond	*c;

			if ((c = calloc(1, sizeof *c)) == NULL)
				fatal("out of memory");
			c->c_type = C_DOM;
			c->c_map = $2;
			$$ = c;
		}
		| ALL				{
			struct cond	*c;

			if ((c = calloc(1, sizeof *c)) == NULL)
				fatal("out of memory");
			c->c_type = C_ALL;
			$$ = c;
		}
		;

condition_list	: condition comma condition_list	{
			TAILQ_INSERT_TAIL(&rule->r_conditions, $1, c_entry);
		}
		;

conditions	: condition				{
			TAILQ_INSERT_TAIL(&rule->r_conditions, $1, c_entry);
		}
		| '{' condition_list '}'
		;

action		: DELIVER TO MAILDIR STRING	{
			rule->r_action = A_MAILDIR;
			if (strlcpy(rule->r_value.path, $4, MAXPATHLEN)
			    >= MAXPATHLEN)
				fatal("pathname too long");
			free($4);
		}
		| DELIVER TO MBOX STRING		{
			rule->r_action = A_MBOX;
			if (strlcpy(rule->r_value.path, $4, MAXPATHLEN)
			    >= MAXPATHLEN)
				fatal("pathname too long");
			free($4);
		}
		| DELIVER TO MDA STRING		{
			rule->r_action = A_EXT;
			if (strlcpy(rule->r_value.command, $4, MAXPATHLEN)
			    >= MAXPATHLEN)
				fatal("command too long");
			free($4);
		}
		| RELAY				{
			rule->r_action = A_RELAY;
		}
		| RELAY VIA STRING PORT NUMBER {
			rule->r_action = A_RELAYVIA;
			if (strlcpy(rule->r_value.host.hostname, $3, MAXHOSTNAMELEN)
			    >= MAXHOSTNAMELEN)
				fatal("hostname too long");
			if ($5 <= 0 || $5 >= (int)USHRT_MAX) {
				yyerror("invalid port: %d", $5);
				YYERROR;
			}
			rule->r_value.host.port = $5;
			free($3);
		}
		;

from		: FROM mapref			{
			$$ = $2;
		}
		| /* empty */			{
			struct map	*m;
			struct mapel	*me;
			struct sockaddr_in *ssin;
			struct sockaddr_in6 *ssin6;
			
			if ((m = calloc(1, sizeof(*m))) == NULL)
				fatal("out of memory");
			m->m_id = last_map_id++;
			if (m->m_id == INT_MAX) {
				yyerror("too many maps defined");
				free(m);
				YYERROR;
			}
			snprintf(m->m_name, STRLEN, "<dynamic(%u)>", m->m_id);
			m->m_flags |= F_DYNAMIC|F_USED;
			m->m_type = T_SINGLE;

			TAILQ_INIT(&m->m_contents);

			if ((me = calloc(1, sizeof(*me))) == NULL)
				fatal("out of memory");
			me->me_key.med_addr.masked = 0;
			ssin = (struct sockaddr_in *)&me->me_key.med_addr.ss;
			ssin->sin_family = AF_INET;
			if (inet_pton(AF_INET, "127.0.0.1", &ssin->sin_addr) != 1) {
				free(me);
				free(m);
				YYERROR;
			}
			TAILQ_INSERT_TAIL(&m->m_contents, me, me_entry);

			if ((me = calloc(1, sizeof(*me))) == NULL)
				fatal("out of memory");
			me->me_key.med_addr.masked = 0;
			ssin6 = (struct sockaddr_in6 *)&me->me_key.med_addr.ss;
			ssin6->sin6_family = AF_INET6;
			if (inet_pton(AF_INET6, "::1", &ssin6->sin6_addr) != 1) {
				free(me);
				free(m);
				YYERROR;
			}
			TAILQ_INSERT_TAIL(&m->m_contents, me, me_entry);

			TAILQ_INSERT_TAIL(conf->sc_maps, m, m_entry);
			$$ = m->m_id;
		}
		;

rule		: decision from			{
			struct rule	*r;

			if ((r = calloc(1, sizeof(*r))) == NULL)
				fatal("out of memory");
			rule = r;
			rule->r_sources = map_find(conf, $2);
			TAILQ_INIT(&rule->r_conditions);
			TAILQ_INIT(&rule->r_options);

		} FOR conditions action	{
			TAILQ_INSERT_TAIL(conf->sc_rules, rule, r_entry);
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
		{ "all",		ALL },
		{ "certificate",	CERTIFICATE },
		{ "config",		CONFIG },
		{ "db",			DB },
		{ "deliver",		DELIVER },
		{ "dns",		DNS },
		{ "domain",		DOMAIN },
		{ "external",		EXTERNAL },
		{ "file",		TFILE },
		{ "for",		FOR },
		{ "from",		FROM },
		{ "hash",		HASH },
		{ "hostname",		HOSTNAME },
		{ "include",		INCLUDE },
		{ "interval",		INTERVAL },
		{ "list",		LIST },
		{ "listen",		LISTEN },
		{ "maildir",		MAILDIR },
		{ "map",		MAP },
		{ "mbox",		MBOX },
		{ "mda",		MDA },
		{ "network",		NETWORK },
		{ "on",			ON },
		{ "port",		PORT },
		{ "queue",		QUEUE },
		{ "reject",		REJECT },
		{ "relay",		RELAY },
		{ "single",		SINGLE },
		{ "source",		SOURCE },
		{ "ssl",		SSL },
		{ "ssmtp",		SSMTP },
		{ "to",			TO },
		{ "type",		TYPE },
		{ "use",		USE },
		{ "via",		VIA },
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
				else if (next == '\n')
					continue;
				else
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

	if ((nfile = calloc(1, sizeof(struct file))) == NULL ||
	    (nfile->name = strdup(name)) == NULL) {
		log_warn("malloc");
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

	conf = x_conf;
	bzero(conf, sizeof(*conf));
	if ((conf->sc_maps = calloc(1, sizeof(*conf->sc_maps))) == NULL ||
	    (conf->sc_rules = calloc(1, sizeof(*conf->sc_rules))) == NULL) {
		log_warn("cannot allocate memory");
		return 0;
	}

	errors = 0;
	last_map_id = 0;

	map = NULL;
	rule = NULL;

	TAILQ_INIT(&conf->sc_listeners);
	TAILQ_INIT(conf->sc_maps);
	TAILQ_INIT(conf->sc_rules);
	SPLAY_INIT(&conf->sc_sessions);
	SPLAY_INIT(&conf->sc_ssl);

	conf->sc_qintval.tv_sec = SMTPD_QUEUE_INTERVAL;
	conf->sc_qintval.tv_usec = 0;
	conf->sc_opts = opts;

	if ((file = pushfile(filename, 0)) == NULL) {
		purge_config(conf, PURGE_EVERYTHING);
		return (-1);
	}
	topfile = file;

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
		purge_config(conf, PURGE_EVERYTHING);
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
host_dns(const char *s, struct listenerlist *al, int max, in_port_t port,
    u_int8_t flags)
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
		(void)strlcpy(h->ssl_cert_name, s, sizeof(h->ssl_cert_name));

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
host(const char *s, struct listenerlist *al, int max, in_port_t port,
    u_int8_t flags)
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
		(void)strlcpy(h->ssl_cert_name, s, sizeof(h->ssl_cert_name));

		TAILQ_INSERT_HEAD(al, h, entry);
		return (1);
	}

	return (host_dns(s, al, max, port, flags));
}
