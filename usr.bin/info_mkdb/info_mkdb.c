/*	$OpenBSD$	*/

/*-
 * Copyright (c) 1996 SigmaSoft, Th. Lockert <tholo@sigmasoft.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by SigmaSoft, Th.  Lockert.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <db.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

void	 db_build __P((char **));
void	 dounlink __P((void));
void	 usage __P((void));
int	 igetnext __P((char **, char **));
int	 main __P((int, char *[]));

DB *infodbp;
int verbose;
char *infoname, buf[8 * 1024];

HASHINFO openinfo = {
	4096,		/* bsize */
	16,		/* ffactor */
	256,		/* nelem */
	2048 * 1024,	/* cachesize */
	NULL,		/* hash() */
	0		/* lorder */
};

/*
 * info_mkdb creates a capability hash database for quick retrieval of capability
 * records.  The database contains 2 types of entries: records and references
 * marked by the first byte in the data.  A record entry contains the actual
 * capability record whereas a reference contains the name (key) under which
 * the correct record is stored.
 */
int
main(argc, argv)
	int argc;
	char *argv[];
{
	int c;

	infoname = NULL;
	while ((c = getopt(argc, argv, "f:v")) != EOF) {
		switch(c) {
		case 'f':
			infoname = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (*argv == NULL)
		usage();

	/*
	 * The database file is the first argument if no name is specified.
	 * Make arrangements to unlink it if exit badly.
	 */
	(void)snprintf(buf, sizeof(buf), "%s.db", infoname ? infoname : *argv);
	if ((infoname = strdup(buf)) == NULL) {
		err(1, "strdup");
		/* NOTREACHED */
	}
	if ((infodbp = dbopen(infoname, O_CREAT | O_TRUNC | O_RDWR,
	    DEFFILEMODE, DB_HASH, &openinfo)) == NULL) {
		err(1, "%s", buf);
		/* NOTREACHED */
	}

	if (atexit(dounlink)) {
		err(1, "atexit");
		/* NOTREACHED */
	}

	db_build(argv);

	if (infodbp->close(infodbp) < 0) {
		err(1, "%s", infoname);
		/* NOTREACHED */
	}
	infoname = NULL;
	exit(0);
	/* NOTREACHED */
}

void
dounlink()
{
	if (infoname != NULL)
		(void)unlink(infoname);
}

/*
 * Any changes to these definitions should be made also in the getcap(3)
 * library routines.
 */
#define RECOK	(char)0
#define TCERR	(char)1
#define SHADOW	(char)2

/*
 * db_build() builds the name and capability databases according to the
 * details above.
 */
void
db_build(ifiles)
	char **ifiles;
{
	DBT key, data;
	recno_t reccnt;
	size_t len, bplen;
	int st;
	char *bp, *p, *t;

	data.data = NULL;
	key.data = NULL;
	for (reccnt = 0, bplen = 0; (st = igetnext(&bp, ifiles)) > 0;) {

		/*
		 * Allocate enough memory to store record, terminating
		 * NULL and one extra byte.
		 */
		len = strlen(bp);
		if (bplen <= len + 2) {
			bplen += MAX(256, len + 2);
			if ((data.data = realloc(data.data, bplen)) == NULL) {
				err(1, "realloc");
				/* NOTREACHED */
			}
		}

		/* Find the end of the name field. */
		if ((p = strchr(bp, ',')) == NULL) {
			warnx("no name field: %.*s", (int)MIN(len, 20), bp);
			continue;
		}

		if (isspace(*bp)) {
			warnx("bad name field: %.*s", (int)MIN(len, 20), bp);
			continue;
		}

		/* First byte of stored record indicates status. */
		switch(st) {
		case 1:
			((char *)(data.data))[0] = RECOK;
			break;
		case 2:
			((char *)(data.data))[0] = TCERR;
			warnx("Record not use expanded: %.*s", (int)(p - bp), bp);
			break;
		}

		/* Create the stored record. */
		(void) memmove(&((u_char *)(data.data))[1], bp, len + 1);
		data.size = len + 2;
		for (t = memchr((char *)data.data + 1, ',', data.size - 1) ; t ; t = memchr(t, ',', data.size - (t - (char *)data.data)))
			*t++ = ':';

		if (memchr((char *)data.data + 1, '\0', data.size - 2)) {
			warnx("NUL in entry: %.*s", (int)MIN(len, 20), bp);
			continue;
		}

		/* Store the record under the name field. */
		key.data = bp;
		key.size = p - bp;

		switch(infodbp->put(infodbp, &key, &data, R_NOOVERWRITE)) {
		case -1:
			err(1, "put");
			/* NOTREACHED */
		case 1:
			warnx("ignored duplicate: %.*s",
			    (int)key.size, (char *)key.data);
			continue;
		}
		++reccnt;

		/* If only one name, ignore the rest. */
		if ((p = strchr(bp, '|')) == NULL)
			continue;

		/* The rest of the names reference the entire name. */
		((char *)(data.data))[0] = SHADOW;
		(void) memmove(&((u_char *)(data.data))[1], key.data, key.size);
		data.size = key.size + 1;

		/* Store references for other names. */
		for (p = t = bp;; ++p) {
			if (p > t && (*p == ',' || *p == '|')) {
				key.size = p - t;
				key.data = t;

				switch(infodbp->put(infodbp,
				    &key, &data, R_NOOVERWRITE)) {
				case -1:
					err(1, "put");
					/* NOTREACHED */
				case 1:
					warnx("ignored duplicate: %.*s",
					    (int)key.size, (char *)key.data);
				}
				t = p + 1;
			}
			if (*p == ',')
				break;
		}
	}

	switch(st) {
	case -1:
		err(1, "file argument");
		/* NOTREACHED */
	case -2:
		errx(1, "potential reference loop detected");
		/* NOTREACHED */
	}

	if (verbose)
		(void)printf("info_mkdb: %d capability records\n", reccnt);
}

void
usage()
{
	(void)fprintf(stderr,
	    "usage: info_mkdb [-v] [-f outfile] file1 [file2 ...]\n");
	exit(1);
}
