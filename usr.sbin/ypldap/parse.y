/*	$OpenBSD: parse.y,v 1.114 2008/05/08 02:15:34 reyk Exp $	*/

/*
 * Copyright (c) 2008 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2007, 2008 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2004, 2005 Esben Norby <norby@openbsd.org>
 * Copyright (c) 2004 Ryan McBride <mcbride@openbsd.org>
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
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ypldap.h"

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

struct env		*conf = NULL;
struct idm		*idm = NULL;
static int		 errors = 0;

typedef struct {
	union {
		int64_t		 number;
		char		*string;
	} v;
	int lineno;
} YYSTYPE;

%}

%token	SERVER FILTER ATTRIBUTE BINDDN MAPS CHANGE DOMAIN PROVIDE
%token	USER GROUP TO EXPIRE HOME SHELL GECOS UID GID INTERVAL
%token	PASSWD NAME FIXED GROUPNAME GROUPPASSWD GROUPGID MAP
%token	INCLUDE DIRECTORY CLASS PORT SSL ERROR GROUPMEMBERS
%token	<v.string>	STRING
%token  <v.number>	NUMBER
%type	<v.number>	opcode attribute port ssl

%%

grammar		: /* empty */
		| grammar '\n'
		| grammar include '\n'
		| grammar varset '\n'
		| grammar directory '\n'
		| grammar main '\n'
		| grammar error '\n'			{ file->errors++; }
		;

nl		: '\n' optnl
		;

optnl		: '\n' optnl
		| /* empty */
		;

include		: INCLUDE STRING			{
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

varset		: STRING '=' STRING			{
			if (symset($1, $3, 0) == -1)
				fatal("cannot store variable");
			free($1);
			free($3);
		}
		;

ssl		: SSL					{ $$ = 1; }
		| /*empty*/				{ $$ = 0; }
		;

port		: PORT NUMBER				{ $$ = $2; }
		| /*empty*/				{ $$ = 0; }
		;

opcode		: GROUP					{ $$ = 0; }
		| PASSWD				{ $$ = 1; }
		;

attribute	: NAME					{ $$ = 0; }
		| PASSWD				{ $$ = 1; }
		| UID					{ $$ = 2; }
		| GID					{ $$ = 3; }
		| CLASS					{ $$ = 4; }
		| CHANGE				{ $$ = 5; }
		| EXPIRE				{ $$ = 6; }
		| GECOS					{ $$ = 7; }
		| HOME					{ $$ = 8; }
		| SHELL					{ $$ = 9; }
		| GROUPNAME				{ $$ = 10; }
		| GROUPPASSWD				{ $$ = 11; }
		| GROUPGID				{ $$ = 12; }
		| GROUPMEMBERS				{ $$ = 13; }
		;

diropt		: BINDDN STRING				{
			idm->idm_flags |= F_NEEDAUTH;
			if (strlcpy(idm->idm_binddn, $2,
			    sizeof(idm->idm_binddn)) >=
			    sizeof(idm->idm_binddn)) {
				yyerror("directory binddn truncated");
				free($2);
				YYERROR;
			}
			free($2);
		}
		| opcode FILTER STRING			{
			if (strlcpy(idm->idm_filters[$1], $3,
			    sizeof(idm->idm_filters[$1])) >=
			    sizeof(idm->idm_filters[$1])) {
				yyerror("filter truncated");
				free($3);
				YYERROR;
			}
			free($3);
		}
		| ATTRIBUTE attribute MAPS TO STRING	{
			if (strlcpy(idm->idm_attrs[$2], $5,
			    sizeof(idm->idm_attrs[$2])) >=
			    sizeof(idm->idm_attrs[$2])) {
				yyerror("attribute truncated");
				free($5);
				YYERROR;
			}
			free($5);
		}
		| FIXED ATTRIBUTE attribute STRING	{
			if (strlcpy(idm->idm_attrs[$3], $4,
			    sizeof(idm->idm_attrs[$3])) >=
			    sizeof(idm->idm_attrs[$3])) {
				yyerror("attribute truncated");
				free($4);
				YYERROR;
			}
			idm->idm_flags |= F_FIXED_ATTR($3);
			free($4);
		}
		;

directory	: DIRECTORY STRING port ssl 		{
			const char	*service;
			struct servent	*srv;

			if ((idm = calloc(1, sizeof(*idm))) == NULL)
				fatal(NULL);

			if (strlcpy(idm->idm_name, $2, sizeof(idm->idm_name)) >=
			    sizeof(idm->idm_name)) {
				yyerror("directory name truncated");
				free(idm);
				free($2);
				YYERROR;
			}

			if ($4) {
				service = "ldaps";
			} else
				service = "ldap";
			srv = getservbyname(service, "tcp");

			if ($3)
				idm->idm_port = $3;
			else if (srv == NULL) {
				if ($4)
					idm->idm_port = 389;
				else
					idm->idm_port = 686;
			} else
				idm->idm_port = ntohs(srv->s_port);
			free($2);
		} '{' optnl diropts '}'			{
			TAILQ_INSERT_TAIL(&conf->sc_idms, idm, idm_entry);
			idm = NULL;
		}
		;

main		: INTERVAL NUMBER			{
			conf->sc_conf_tv.tv_sec = $2;
			conf->sc_conf_tv.tv_usec = 0;
		}
		| DOMAIN STRING				{
			if (strlcpy(conf->sc_domainname, $2,
			    sizeof(conf->sc_domainname)) >=
			    sizeof(conf->sc_domainname)) {
				yyerror("domainname truncated");
				free($2);
				YYERROR;
			}
			free($2);
		}
		| PROVIDE MAP STRING			{
			if (strcmp($3, "passwd.byname") == 0)
				conf->sc_flags |= YPMAP_PASSWD_BYNAME;
			else if (strcmp($3, "passwd.byuid") == 0)
				conf->sc_flags |= YPMAP_PASSWD_BYUID;
			else if (strcmp($3, "master.passwd.byname") == 0)
				conf->sc_flags |= YPMAP_MASTER_PASSWD_BYNAME;
			else if (strcmp($3, "master.passwd.byuid") == 0)
				conf->sc_flags |= YPMAP_MASTER_PASSWD_BYUID;
			else if (strcmp($3, "group.byname") == 0)
				conf->sc_flags |= YPMAP_GROUP_BYNAME;
			else if (strcmp($3, "group.bygid") == 0)
				conf->sc_flags |= YPMAP_GROUP_BYGID;
			else {
				yyerror("unsupported map type: %s", $3);
				free($3);
				YYERROR;
			}
			free($3);
		}
		;

diropts		: diropts diropt nl
		| diropt optnl
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
		{ "attribute",		ATTRIBUTE },
		{ "binddn",		BINDDN },
		{ "change",		CHANGE },
		{ "class",		CLASS },
		{ "directory",		DIRECTORY },
		{ "domain",		DOMAIN },
		{ "expire",		EXPIRE },
		{ "filter",		FILTER },
		{ "fixed",		FIXED },
		{ "gecos",		GECOS },
		{ "gid",		GID },
		{ "group",		GROUP },
		{ "groupgid",		GROUPGID },
		{ "groupmembers",	GROUPMEMBERS },
		{ "groupname",		GROUPNAME },
		{ "grouppasswd",	GROUPPASSWD },
		{ "home",		HOME },
		{ "include",		INCLUDE },
		{ "interval",		INTERVAL },
		{ "map",		MAP },
		{ "maps",		MAPS },
		{ "name",		NAME },
		{ "passwd",		PASSWD },
		{ "port",		PORT },
		{ "provide",		PROVIDE },
		{ "server",		SERVER },
		{ "shell",		SHELL },
		{ "ssl",		SSL },
		{ "to",			TO },
		{ "uid",		UID },
		{ "user",		USER },
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
parse_config(struct env *x_conf, const char *filename, int opts)
{
	struct sym	*sym, *next;

	conf = x_conf;
	bzero(conf, sizeof(*conf));

	TAILQ_INIT(&conf->sc_idms);
	conf->sc_conf_tv.tv_sec = DEFAULT_INTERVAL;
	conf->sc_conf_tv.tv_usec = 0;

	errors = 0;

	if ((file = pushfile(filename, 0)) == NULL) {
		return (-1);
	}
	topfile = file;

	/*
	 * parse configuration
	 */
	setservent(1);
	yyparse();
	endservent();
	errors = file->errors;
	popfile();

	/* Free macros and check which have not been used. */
	for (sym = TAILQ_FIRST(&symhead); sym != NULL; sym = next) {
		next = TAILQ_NEXT(sym, entry);
		if ((opts & YPLDAP_OPT_VERBOSE) && !sym->used)
			fprintf(stderr, "warning: macro '%s' not "
			    "used\n", sym->nam);
		if (!sym->persist) {
			free(sym->nam);
			free(sym->val);
			TAILQ_REMOVE(&symhead, sym, entry);
			free(sym);
		}
	}

	if (errors) {
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
