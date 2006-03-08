/*	$OpenBSD: rcsmerge.c,v 1.17 2006/03/07 01:40:52 joris Exp $	*/
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

static int kflag = RCS_KWEXP_ERR;

int
rcsmerge_main(int argc, char **argv)
{
	int i, ch;
	char *fcont, fpath[MAXPATHLEN], r1[16], r2[16];
	RCSFILE *file;
	RCSNUM *baserev, *rev2, *frev;
	BUF *bp;

	baserev = rev2 = RCS_HEAD_REV;

	while ((ch = rcs_getopt(argc, argv, "AEek:p::q::r:TVx:z:")) != -1) {
		switch (ch) {
		case 'A': case 'E': case 'e':
			break;
		case 'k':
			kflag = rcs_kflag_get(rcs_optarg);
			if (RCS_KWEXP_INVAL(kflag)) {
				cvs_log(LP_ERR,
				    "invalid RCS keyword expansion mode");
				(usage)();
				exit(1);
			}
			break;
		case 'p':
			rcs_set_rev(rcs_optarg, &baserev);
			pipeout = 1;
			break;
		case 'q':
			rcs_set_rev(rcs_optarg, &baserev);
			verbose = 0;
			break;
		case 'r':
			if (baserev == RCS_HEAD_REV)
				rcs_set_rev(rcs_optarg, &baserev);
			else if (rev2 == RCS_HEAD_REV)
				rcs_set_rev(rcs_optarg, &rev2);
			else
				fatal("too many revision numbers");
			break;
		case 'T':
			/*
			 * kept for compatibility
			 */
			break;
		case 'V':
			printf("%s\n", rcs_version);
			exit(0);
		case 'x':
			rcs_suffixes = rcs_optarg;
			break;
		case 'z':
			timezone_flag = rcs_optarg;
			break;
		default:
			break;
		}
	}

	argc -= rcs_optind;
	argv += rcs_optind;

	if (argc < 0) {
		cvs_log(LP_ERR, "no input file");
		(usage)();
		exit(1);
	}

	if (baserev == RCS_HEAD_REV) {
		cvs_log(LP_ERR, "no base revision number given");
		(usage)();
		exit(1);
	}

	for (i = 0; i < argc; i++) {
		if (rcs_statfile(argv[i], fpath, sizeof(fpath)) < 0)
			continue;

		if ((file = rcs_open(fpath, RCS_READ)) == NULL)
			continue;

		if (verbose == 1)
			fprintf(stderr, "RCS file: %s\n", fpath);

		if (rev2 == RCS_HEAD_REV)
			frev = file->rf_head;
		else
			frev = rev2;

		if (rcsnum_cmp(baserev, frev, 0) == 0) {
			rcs_close(file);
			continue;
		}

		rcsnum_tostr(baserev, r1, sizeof(r1));
		rcsnum_tostr(frev, r2, sizeof(r2));

		if (verbose == 1)
			fprintf(stderr, "Merging differences between %s and "
			    "%s into %s%s\n", r1, r2, argv[i],
			    (pipeout == 1) ? "; result to stdout":"");

		if ((bp = cvs_diff3(file, argv[i], baserev, frev)) == NULL) {
			cvs_log(LP_ERR, "failed to merge");
			rcs_close(file);
			continue;
		}

		if (pipeout == 1) {
			if (cvs_buf_putc(bp, '\0') < 0) {
				rcs_close(file);
				continue;
			}

			fcont = cvs_buf_release(bp);
			printf("%s", fcont);
			xfree(fcont);
		} else {
			/* XXX mode */
			if (cvs_buf_write(bp, argv[i], 0644) < 0)
				cvs_log(LP_ERR, "failed to write new file");

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
	    "usage: rcsmerge [-AEeV] [-kmode] [-p[rev]] [-q[rev]] "
	    "[-rrev] [-xsuffixes] [-ztz] file ...\n");
}
