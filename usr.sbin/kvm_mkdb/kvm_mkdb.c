/*	$OpenBSD: kvm_mkdb.c,v 1.3 1998/08/19 06:47:53 millert Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "from: @(#)kvm_mkdb.c	8.3 (Berkeley) 5/4/95";
#else
static char *rcsid = "$OpenBSD: kvm_mkdb.c,v 1.3 1998/08/19 06:47:53 millert Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <db.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

static void usage __P((void));

HASHINFO openinfo = {
	4096,		/* bsize */
	128,		/* ffactor */
	1024,		/* nelem */
	2048 * 1024,	/* cachesize */
	NULL,		/* hash() */
	0		/* lorder */
};

int
main(argc, argv)
	int argc;
	char *argv[];
{
	DB *db;
	int ch, verbose = 0;
	char *nlistpath, *nlistname, dbtemp[MAXPATHLEN], dbname[MAXPATHLEN];

	while ((ch = getopt(argc, argv, "v")) != -1)
		switch (ch) {
		case 'v':
			verbose = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc > 1)
		usage();

	nlistpath = argc > 0 ? argv[0] : _PATH_UNIX;
	nlistname = basename(nlistpath);

	(void)snprintf(dbtemp, sizeof(dbtemp), "%skvm_%s.tmp",
	    _PATH_VARDB, nlistname);
	(void)snprintf(dbname, sizeof(dbname), "%skvm_%s.db",
	    _PATH_VARDB, nlistname);

	/* If the existing db file matches the currently running kernel, exit */
	if (testdb(dbname))
		exit(0);
	else if (verbose)
		warnx("rebuilding %s", dbname);

	(void)umask(0);
	db = dbopen(dbtemp, O_CREAT | O_EXLOCK | O_TRUNC | O_RDWR,
	    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH, DB_HASH, &openinfo);
	if (db == NULL)
		err(1, "can't dbopen %s", dbtemp);
	if (create_knlist(nlistpath, db) != 0) {
		(void)unlink(dbtemp);
		errx(1, "cannot determine executable type of %s", nlistpath);
	}
	if (db->close(db)) {
		warn("can't dbclose %s", dbtemp);
		(void)unlink(dbtemp);
		exit(1);
	}
	if (rename(dbtemp, dbname))
		err(1, "rename %s to %s", dbtemp, dbname);
	exit(0);
}

void
usage()
{
	(void)fprintf(stderr, "usage: kvm_mkdb [file]\n");
	exit(1);
}
