/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/tree.h>
#include <sys/queue.h>
#include <sys/param.h>

#include <sys/socket.h>

#include <ctype.h>
#include <db.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "smtpd.h"

extern char *__progname;

__dead void	usage(void);
int		parse_aliases(const char *);
int		parse_entry(char *, size_t, size_t);

DB *db;

int
main(int argc, char *argv[])
{
	char dbname[MAXPATHLEN];
	int ch;

	if (argc != 1)
		usage();

	if (strlcpy(dbname, PATH_ALIASESDB ".XXXXXXXXXXX", MAXPATHLEN)
	    >= MAXPATHLEN)
		errx(1, "path truncation");
	if (mkstemp(dbname) == -1)
		err(1, "mkstemp");

	db = dbopen(dbname, O_EXLOCK|O_RDWR|O_SYNC, 0644, DB_HASH, NULL);
	if (db == NULL) {
		warn("dbopen: %s", dbname);
		goto bad;
	}

	if (! parse_aliases(PATH_ALIASES))
		goto bad;

	if (db->close(db) == -1) {
		warn("dbclose: %s", dbname);
		goto bad;
	}

	if (chmod(dbname, 0644) == -1) {
		warn("chmod: %s", dbname);
		goto bad;
	}

	if (rename(dbname, PATH_ALIASESDB) == -1) {
		warn("rename");
		goto bad;
	}

	return 0;
bad:
	unlink(dbname);
	return 1;
}

int
parse_aliases(const char *filename)
{
	FILE *fp;
	char *line;
	size_t len;
	size_t lineno = 0;
	char delim[] = { '\\', '\\', '#' };

	fp = fopen(filename, "r");
	if (fp == NULL) {
		warn("%s", filename);
		return 0;
	}

	while ((line = fparseln(fp, &len, &lineno, delim, 0)) != NULL) {
		if (len == 0)
			continue;
		if (! parse_entry(line, len, lineno)) {
			free(line);
			fclose(fp);
			return 0;
		}
		free(line);
	}

	fclose(fp);
	return 1;
}

int
parse_entry(char *line, size_t len, size_t lineno)
{
	char *name;
	char *rcpt;
	char *endp;
	char *subrcpt;
	DBT key;
	DBT val;

	name = line;
	rcpt = strchr(line, ':');
	if (rcpt == NULL)
		goto bad;
	*rcpt++ = '\0';

	/* name: strip initial whitespace. */
	while (isspace(*name))
		++name;
	if (*name == '\0')
		goto bad;

	/* name: strip trailing whitespace. */
	endp = name + strlen(name) - 1;
	while (name < endp && isspace(*endp))
		*endp-- = '\0';

	/* At this point name and rcpt are non-zero nul-terminated strings. */
	while ((subrcpt = strsep(&rcpt, ",")) != NULL) {
		struct alias	 alias;
		void		*p;

		/* subrcpt: strip initial whitespace. */
		while (isspace(*subrcpt))
			++subrcpt;
		if (*subrcpt == '\0')
			goto bad;

		/* subrcpt: strip trailing whitespace. */
		endp = subrcpt + strlen(subrcpt) - 1;
		while (subrcpt < endp && isspace(*endp))
			*endp-- = '\0';

		if (! alias_parse(&alias, subrcpt))
			goto bad;

		key.data = name;
		key.size = strlen(name) + 1;
		val.data = NULL;
		val.size = 0;
		if (db->get(db, &key, &val, 0) == -1) {
			warn("dbget");
			return 0;
		}

		p = calloc(1, val.size + sizeof(struct alias));
		if (p == NULL) {
			warn("calloc");
			return 0;
		}
		memcpy(p, val.data, val.size);
		memcpy((u_int8_t *)p + val.size, &alias, sizeof(struct alias));

		val.data = p;
		val.size += sizeof(struct alias);
		if (db->put(db, &key, &val, 0) == -1) {
			warn("dbput");
			free(p);
			return 0;
		}

		free(p);
	}

	return 1;

bad:
	/* The actual line is not printed; it may be mangled by above code. */
	warnx("%s:%zd: invalid entry", PATH_ALIASES, lineno);
	return 0;
}

void
usage(void)
{
	fprintf(stderr, "usage: %s\n", __progname);
	exit(1);
}
