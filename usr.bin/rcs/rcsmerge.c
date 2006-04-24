/*	$OpenBSD: rcsmerge.c,v 1.31 2006/04/24 08:10:41 xsa Exp $	*/
/*
 * Copyright (c) 2005, 2006 Xavier Santolaria <xsa@openbsd.org>
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

int
rcsmerge_main(int argc, char **argv)
{
	int i, ch, flags, kflag;
	char *fcont, fpath[MAXPATHLEN], r1[16], r2[16], *rev_str1, *rev_str2;
	RCSFILE *file;
	RCSNUM *rev1, *rev2;
	BUF *bp;

	flags = 0;
	kflag = RCS_KWEXP_ERR;
	rev1 = rev2 = NULL;
	rev_str1 = rev_str2 = NULL;

	while ((ch = rcs_getopt(argc, argv, "AEek:p::q::r:TVx::z:")) != -1) {
		switch (ch) {
		case 'A': case 'E': case 'e':
			break;
		case 'k':
			kflag = rcs_kflag_get(rcs_optarg);
			if (RCS_KWEXP_INVAL(kflag)) {
				warnx("invalid RCS keyword substitution mode");
				(usage)();
				exit(1);
			}
			break;
		case 'p':
			rcs_setrevstr2(&rev_str1, &rev_str2, rcs_optarg);
			flags |= PIPEOUT;
			break;
		case 'q':
			rcs_setrevstr2(&rev_str1, &rev_str2, rcs_optarg);
			flags |= QUIET;
			break;
		case 'r':
			rcs_setrevstr2(&rev_str1, &rev_str2, rcs_optarg);
			break;
		case 'T':
			/*
			 * kept for compatibility
			 */
			break;
		case 'V':
			printf("%s\n", rcs_version);
			exit(0);
			/* NOTREACHED */
		case 'x':
			/* Use blank extension if none given. */
			rcs_suffixes = rcs_optarg ? rcs_optarg : "";
			break;
		case 'z':
			timezone_flag = rcs_optarg;
			break;
		default:
			(usage)();
			exit(1);
		}
	}

	argc -= rcs_optind;
	argv += rcs_optind;

	if (argc < 0) {
		warnx("no input file");
		(usage)();
		exit(1);
	}

	if (rev_str1 == NULL) {
		warnx("no base revision number given");
		(usage)();
		exit(1);
	}

	for (i = 0; i < argc; i++) {
		if (rcs_statfile(argv[i], fpath, sizeof(fpath), flags) < 0)
			continue;

		if ((file = rcs_open(fpath, RCS_READ)) == NULL)
			continue;

		if (!(flags & QUIET))
			fprintf(stderr, "RCS file: %s\n", fpath);

		if (rev1 != NULL) {
			rcsnum_free(rev1);
			rev1 = NULL;
		}
		if (rev2 != NULL) {
			rcsnum_free(rev2);
			rev2 = NULL;
		}

		if ((rev1 = rcs_getrevnum(rev_str1, file)) == NULL)
			fatal("invalid revision: %s", rev_str1);
		if (rev_str2 != NULL) {
			if ((rev2 = rcs_getrevnum(rev_str2, file)) == NULL)
				fatal("invalid revision: %s", rev_str2);
		} else {
			rev2 = rcsnum_alloc();
			rcsnum_cpy(file->rf_head, rev2, 0);
		}

		if (rcsnum_cmp(rev1, rev2, 0) == 0) {
			rcs_close(file);
			continue;
		}

		if (!(flags & QUIET)) {
			(void)rcsnum_tostr(rev1, r1, sizeof(r1));
			(void)rcsnum_tostr(rev2, r2, sizeof(r2));

			fprintf(stderr, "Merging differences between %s and "
			    "%s into %s%s\n", r1, r2, argv[i],
			    (flags & PIPEOUT) ? "; result to stdout":"");
		}

		if ((bp = cvs_diff3(file, argv[i], rev1, rev2,
		    !(flags & QUIET))) == NULL) {
			warnx("failed to merge");
			rcs_close(file);
			continue;
		}

		if (flags & PIPEOUT) {
			cvs_buf_putc(bp, '\0');
			fcont = cvs_buf_release(bp);
			printf("%s", fcont);
			xfree(fcont);
		} else {
			/* XXX mode */
			if (cvs_buf_write(bp, argv[i], 0644) < 0)
				warnx("cvs_buf_write failed");

			cvs_buf_free(bp);
		}
		rcs_close(file);
	}

	return (0);
}

void
rcsmerge_usage(void)
{
	fprintf(stderr,
	    "usage: rcsmerge [-AEeV] [-kmode] [-p[rev]] [-q[rev]]\n"
	    "                [-xsuffixes] [-ztz] -rrev file ...\n");
}
