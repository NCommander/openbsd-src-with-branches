/*	$OpenBSD: rcsclean.c,v 1.32 2006/04/13 16:23:31 ray Exp $	*/
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

#include "includes.h"

#include "rcsprog.h"
#include "diff.h"

static void	rcsclean_file(char *, const char *);

static int nflag = 0;
static int kflag = RCS_KWEXP_ERR;
static int uflag = 0;
static int flags = 0;
static char *locker = NULL;

int
rcsclean_main(int argc, char **argv)
{
	int i, ch;
	char *rev_str;
	DIR *dirp;
	struct dirent *dp;

	rev_str = NULL;

	while ((ch = rcs_getopt(argc, argv, "k:n::q::r:Tu::Vx::")) != -1) {
		switch (ch) {
		case 'k':
			kflag = rcs_kflag_get(rcs_optarg);
			if (RCS_KWEXP_INVAL(kflag)) {
				cvs_log(LP_ERR,
				    "invalid RCS keyword expansion mode");
				(usage)();
				exit(1);
			}
			break;
		case 'n':
			rcs_setrevstr(&rev_str, rcs_optarg);
			nflag = 1;
			break;
		case 'q':
			rcs_setrevstr(&rev_str, rcs_optarg);
			verbose = 0;
			break;
		case 'r':
			rcs_setrevstr(&rev_str, rcs_optarg);
			break;
		case 'T':
			flags |= PRESERVETIME;
			break;
		case 'u':
			rcs_setrevstr(&rev_str, rcs_optarg);
			uflag = 1;
			break;
		case 'V':
			printf("%s\n", rcs_version);
			exit(0);
			/* NOTREACHED */
		case 'x':
			/* Use blank extension if none given. */
			rcs_suffixes = rcs_optarg ? rcs_optarg : "";
			break;
		default:
			break;
		}
	}

	argc -= rcs_optind;
	argv += rcs_optind;

	if ((locker = getlogin()) == NULL)
		fatal("getlogin failed");

	if (argc == 0) {
		if ((dirp = opendir(".")) == NULL) {
			cvs_log(LP_ERRNO, "failed to open directory '.'");
			(usage)();
			exit(1);
		}

		while ((dp = readdir(dirp)) != NULL) {
			if (dp->d_type == DT_DIR)
				continue;
			rcsclean_file(dp->d_name, rev_str);
		}

		closedir(dirp);
	} else
		for (i = 0; i < argc; i++)
			rcsclean_file(argv[i], rev_str);

	return (0);
}

void
rcsclean_usage(void)
{
	fprintf(stderr,
	    "usage: rcsclean [-TV] [-kmode] [-n[rev]] [-q[rev]]\n"
	    "                [-rrev] [-u[rev]] [-xsuffixes] [-ztz] [file] ...\n");
}

static void
rcsclean_file(char *fname, const char *rev_str)
{
	int match;
	RCSFILE *file;
	char fpath[MAXPATHLEN], numb[64];
	RCSNUM *rev;
	BUF *b1, *b2;
	char *c1, *c2;
	struct stat st;
	time_t rcs_mtime = -1;

	match = 1;

	if (stat(fname, &st) == -1)
		return;

	if (rcs_statfile(fname, fpath, sizeof(fpath)) < 0)
		return;

	if ((file = rcs_open(fpath, RCS_RDWR)) == NULL)
		return;

	if (flags & PRESERVETIME)
		rcs_mtime = rcs_get_mtime(file->rf_path);

	rcs_kwexp_set(file, kflag);

	if (rev_str == NULL)
		rev = file->rf_head;
	else if ((rev = rcs_getrevnum(rev_str, file)) == NULL) {
		cvs_log(LP_ERR, "%s: Symbolic name `%s' is undefined.",
		    fpath, rev_str);
		rcs_close(file);
		return;
	}

	if ((b1 = rcs_getrev(file, rev)) == NULL) {
		cvs_log(LP_ERR, "failed to get needed revision");
		rcs_close(file);
		return;
	}

	if ((b2 = cvs_buf_load(fname, BUF_AUTOEXT)) == NULL) {
		cvs_log(LP_ERRNO, "failed to load '%s'", fname);
		rcs_close(file);
		return;
	}

	cvs_buf_putc(b1, '\0');
	cvs_buf_putc(b2, '\0');

	c1 = cvs_buf_release(b1);
	c2 = cvs_buf_release(b2);

	/* XXX - Compare using cvs_buf_len() first. */
	if (strcmp(c1, c2) != 0)
		match = 0;

	xfree(c1);
	xfree(c2);

	if (match == 1) {
		if (uflag == 1 && !TAILQ_EMPTY(&(file->rf_locks))) {
			if (verbose == 1 && nflag == 0) {
				printf("rcs -u%s %s\n",
				    rcsnum_tostr(rev, numb, sizeof(numb)),
				    fpath);
			}
			(void)rcs_lock_remove(file, locker, rev);
		}

		if (TAILQ_EMPTY(&(file->rf_locks))) {
			if (verbose == 1)
				printf("rm -f %s\n", fname);

			if (nflag == 0)
				(void)unlink(fname);
		}
	}

	rcs_close(file);

	if (flags & PRESERVETIME)
		rcs_set_mtime(fpath, rcs_mtime);
}
