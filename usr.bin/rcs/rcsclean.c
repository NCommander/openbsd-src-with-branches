/*	$OpenBSD: rcsclean.c,v 1.2 2005/10/06 02:12:53 joris Exp $	*/
/*
 * Copyright (c) 2005 Joris Vink <joris@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/stat.h>

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "diff.h"
#include "log.h"
#include "rcs.h"
#include "rcsprog.h"

extern char *__progname;
static int rcsclean_file(char *, RCSNUM *);
static int nflag = 0;
static int kflag = RCS_KWEXP_ERR;

int
rcsclean_main(int argc, char **argv)
{
	int i, ch;
	RCSNUM *rev;
	DIR *dirp;
	struct dirent *dp;

	rev = RCS_HEAD_REV;

	while ((ch = getopt(argc, argv, "k:nqr:V")) != -1) {
		switch (ch) {
		case 'k':
			kflag = rcs_kflag_get(optarg);
			if (RCS_KWEXP_INVAL(kflag)) {
				cvs_log(LP_ERR,
				    "invalid RCS keyword expansion mode");
				(usage)();
				exit(1);
			}
			break;
		case 'n':
			nflag = 1;
			break;
		case 'q':
			verbose = 0;
			break;
		case 'r':
			if ((rev = rcsnum_parse(optarg)) == NULL) {
				cvs_log(LP_ERR, "bad revision number");
				exit(1);
			}
			break;
		case 'V':
			printf("%s\n", rcs_version);
			exit(0);
		default:
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		if ((dirp = opendir(".")) == NULL) {
			cvs_log(LP_ERRNO, "failed to open directory '.'");
			(usage)();
			exit(1);
		}

		while ((dp = readdir(dirp)) != NULL) {
			if (dp->d_type == DT_DIR)
				continue;
			rcsclean_file(dp->d_name, rev);
		}

		closedir(dirp);
	} else {
		for (i = 0; i < argc; i++)
			rcsclean_file(argv[i], rev);
	}

	return (0);
}

void
rcsclean_usage(void)
{
	fprintf(stderr, "usage %s [-qV] [-r rev] [file] ...\n", __progname);
}

static int
rcsclean_file(char *fname, RCSNUM *rev)
{
	int match;
	RCSFILE *file;
	char fpath[MAXPATHLEN];
	RCSNUM *frev;
	BUF *b1, *b2;
	char *s1, *s2, *c1, *c2;
	struct stat st;

	match = 1;

	if (stat(fname, &st) == -1)
		return (-1);

	if (rcs_statfile(fname, fpath, sizeof(fpath)) < 0)
		return (-1);

	if ((file = rcs_open(fpath, RCS_RDWR)) == NULL)
		return (-1);

	if (!RCS_KWEXP_INVAL(kflag))
		rcs_kwexp_set(file, kflag);

	if (rev == RCS_HEAD_REV)
		frev = file->rf_head;
	else
		frev = rev;

	if ((b1 = rcs_getrev(file, frev)) == NULL) {
		cvs_log(LP_ERR, "failed to get needed revision");
		rcs_close(file);
		return (-1);
	}

	if ((b2 = cvs_buf_load(fname, BUF_AUTOEXT)) == NULL) {
		cvs_log(LP_ERRNO, "failed to load '%s'", fname);
		rcs_close(file);
		return (-1);
	}

	cvs_buf_putc(b1, '\0');
	cvs_buf_putc(b2, '\0');

	c1 = cvs_buf_release(b1);
	c2 = cvs_buf_release(b2);

	for (s1 = c1, s2 = c2; *s1 && *s2; *s1++, *s2++) {
		if (*s1 != *s2) {
			match = 0;
			break;
		}
	}

	free(c1);
	free(c2);

	if (match) {
		if (verbose)
			printf("rm -f %s\n", fname);
		if (nflag == 0)
			(void)unlink(fname);
	}

	rcs_close(file);
	return (0);
}
