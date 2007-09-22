/*	$OpenBSD: annotate.c,v 1.38 2007/09/13 13:10:57 tobias Exp $	*/
/*
 * Copyright (c) 2006 Xavier Santolaria <xsa@openbsd.org>
 * Copyright (c) 2007 Tobias Stoeckmann <tobias@openbsd.org>
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

#include <sys/param.h>
#include <sys/dirent.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cvs.h"
#include "remote.h"

void	cvs_annotate_local(struct cvs_file *);

static int	 force_head = 0;
static char	*rev = NULL;

struct cvs_cmd cvs_cmd_annotate = {
	CVS_OP_ANNOTATE, 0, "annotate",
	{ "ann", "blame" },
	"Show last revision where each line was modified",
	"[-flR] [-D date | -r rev] [file ...]",
	"D:flRr:",
	NULL,
	cvs_annotate
};

int
cvs_annotate(int argc, char **argv)
{
	int ch, flags;
	char *arg = ".";
	struct cvs_recursion cr;

	flags = CR_RECURSE_DIRS;

	while ((ch = getopt(argc, argv, cvs_cmd_annotate.cmd_opts)) != -1) {
		switch (ch) {
		case 'D':
			break;
		case 'f':
			force_head = 1;
			break;
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'R':
			break;
		case 'r':
			rev = optarg;
			break;
		default:
			fatal("%s", cvs_cmd_annotate.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	cr.enterdir = NULL;
	cr.leavedir = NULL;

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_connect_to_server();
		cr.fileproc = cvs_client_sendfile;

		if (force_head == 1)
			cvs_client_send_request("Argument -f");

		if (!(flags & CR_RECURSE_DIRS))
			cvs_client_send_request("Argument -l");

		if (rev != NULL)
			cvs_client_send_request("Argument -r%s", rev);
	} else {
		cr.fileproc = cvs_annotate_local;
	}

	cr.flags = flags;

	if (argc > 0)
		cvs_file_run(argc, argv, &cr);
	else
		cvs_file_run(1, &arg, &cr);

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");
		cvs_client_send_request("annotate");
		cvs_client_get_responses();
	}

	return (0);
}

void
cvs_annotate_local(struct cvs_file *cf)
{
	int i;
	char date[10], rnum[13], *p;
	RCSNUM *crev;
	struct cvs_line **alines;

	cvs_log(LP_TRACE, "cvs_annotate_local(%s)", cf->file_path);

	cvs_file_classify(cf, cvs_directory_tag);

	if (cf->file_status == FILE_UNKNOWN || cf->file_status == FILE_UNLINK ||
	    cf->file_type != CVS_FILE)
		return;

	if (rev == NULL)
		rcs_rev_getlines(cf->file_rcs, cf->file_rcsrev, &alines);
	else {
		crev = rcsnum_parse(rev);

		if (rcsnum_cmp(crev, cf->file_rcsrev, 0) < 0) {
			if (!force_head) {
				/* Stick at weird GNU cvs, ignore error. */
				rcsnum_free(crev);
				return;
			}
			rcsnum_cpy(cf->file_rcsrev, crev, 0);
		}
		rcs_rev_getlines(cf->file_rcs, crev, &alines);
		rcsnum_free(crev);
	}

	/* Stick at weird GNU cvs, ignore error. */
	if (alines == NULL)
		return;

	cvs_log(LP_RCS, "Annotations for %s", cf->file_path);
	cvs_log(LP_RCS, "***************");

	for (i = 0; alines[i] != NULL; i++) {
		rcsnum_tostr(alines[i]->l_delta->rd_num, rnum, sizeof(rnum));
		strftime(date, sizeof(date), "%d-%b-%y",
		    &(alines[i]->l_delta->rd_date));
		if (alines[i]->l_len &&
		    alines[i]->l_line[alines[i]->l_len - 1] == '\n')
			alines[i]->l_line[alines[i]->l_len - 1] = '\0';
		else {
			p = xmalloc(alines[i]->l_len + 1);
			memcpy(p, alines[i]->l_line, alines[i]->l_len);
			p[alines[i]->l_len] = '\0';

			if (alines[i]->l_needsfree)
				xfree(alines[i]->l_line);
			alines[i]->l_line = p;
			alines[i]->l_len++;
			alines[i]->l_needsfree = 1;
		}
		cvs_printf("%-12.12s (%-8.8s %s): %s\n", rnum,
		    alines[i]->l_delta->rd_author, date, alines[i]->l_line);

		if (alines[i]->l_needsfree)
			xfree(alines[i]->l_line);
		xfree(alines[i]);
	}

	xfree(alines);
}
