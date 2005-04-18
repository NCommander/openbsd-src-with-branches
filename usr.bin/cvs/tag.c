/*	$OpenBSD: tag.c,v 1.12 2005/04/16 20:31:18 xsa Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * Copyright (c) 2004 Joris Vink <joris@openbsd.org>
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

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "cvs.h"
#include "log.h"
#include "proto.h"


int cvs_tag_file(CVSFILE *, void *);
int cvs_tag_options(char *, int, char **, int *);
int cvs_tag_sendflags(struct cvsroot *);

static char *tag, *old_tag, *date;
static int branch, delete;

struct cvs_cmd_info cvs_tag = {
	cvs_tag_options,
	cvs_tag_sendflags,
	cvs_tag_file,	
	NULL, NULL,
	CF_SORT | CF_IGNORE | CF_RECURSE,
	CVS_REQ_TAG,
	CVS_CMD_ALLOWSPEC
};

int
cvs_tag_options(char *opt, int argc, char **argv, int *arg)
{
	int ch;

	date = old_tag = NULL;
	branch = delete = 0;

	while ((ch = getopt(argc, argv, opt)) != -1) {
		switch (ch) {
		case 'b':
			branch = 1;
			break;
		case 'd':
			delete = 1;
			break;
		case 'D':
			date = optarg;
			break;
		case 'l':
			cvs_tag.file_flags &= ~CF_RECURSE;
			break;
		case 'r':
			old_tag = optarg;
			break;
		default:
			return (CVS_EX_USAGE);
		}
	}

	*arg = optind;
	argc -= optind;
	argv += optind;

	if (argc == 0) {
		return (CVS_EX_USAGE);
	} else {
		tag = argv[0];
		argc--;
		argv++;
		*arg += 1;
	}

	if (branch && delete) {
		cvs_log(LP_WARN, "ignoring -b with -d options");
		branch = 0;
	}

	if (delete && old_tag)
		old_tag = NULL;

	if (delete && date)
		date = NULL;

	if (old_tag != NULL && date != NULL) {
		cvs_log(LP_ERROR, "-r and -D options are mutually exclusive");
		return (CVS_EX_USAGE);
	}

	return (0);
}

int
cvs_tag_sendflags(struct cvsroot *root)
{
	if (branch && (cvs_sendarg(root, "-b", 0) < 0))
		return (CVS_EX_PROTO);

	if (delete && (cvs_sendarg(root, "-d", 0) < 0))
		return (CVS_EX_PROTO);

	if (old_tag) {
		if ((cvs_sendarg(root, "-r", 0) < 0) ||
		    (cvs_sendarg(root, old_tag, 0) < 0))
			return (CVS_EX_PROTO);
	}

	if (date) {
		if ((cvs_sendarg(root, "-D", 0) < 0) ||
		    (cvs_sendarg(root, date, 0) < 0))
			return (CVS_EX_PROTO);
	}

	if (cvs_sendarg(root, tag, 0) < 0)
		return (CVS_EX_PROTO);

	return (0);
}


/*
 * cvs_tag_file()
 *
 * Get the status of a single file.
 */
int
cvs_tag_file(CVSFILE *cfp, void *arg)
{
	int ret, l;
	char *repo, fpath[MAXPATHLEN], rcspath[MAXPATHLEN];
	RCSFILE *rf;
	struct cvsroot *root;

	ret = 0;
	rf = NULL;
	root = CVS_DIR_ROOT(cfp);

	if ((root->cr_method != CVS_METHOD_LOCAL) && (cfp->cf_type == DT_DIR)) {
		if (cvs_senddir(root, cfp) < 0)
			return (CVS_EX_PROTO);
		return (0);
	}

	cvs_file_getpath(cfp, fpath, sizeof(fpath));

	if (root->cr_method != CVS_METHOD_LOCAL) {
		if (cvs_sendentry(root, cfp) < 0) {
			return (CVS_EX_PROTO);
		}

		switch (cfp->cf_cvstat) {
		case CVS_FST_UNKNOWN:
			ret = cvs_sendreq(root, CVS_REQ_QUESTIONABLE,
			    CVS_FILE_NAME(cfp));
			break;
		case CVS_FST_UPTODATE:
			ret = cvs_sendreq(root, CVS_REQ_UNCHANGED,
			    CVS_FILE_NAME(cfp));
			break;
		case CVS_FST_MODIFIED:
			ret = cvs_sendreq(root, CVS_REQ_ISMODIFIED, 
			    CVS_FILE_NAME(cfp));
		default:
			break;
		}
	} else {
		if (cfp->cf_cvstat == CVS_FST_UNKNOWN) {
			cvs_log(LP_WARN, "I know nothing about %s", fpath);
			return (0);
		}

		repo = CVS_DIR_REPO(cfp);
		l = snprintf(rcspath, sizeof(rcspath), "%s/%s/%s%s",
		    root->cr_dir, repo, CVS_FILE_NAME(cfp), RCS_FILE_EXT);
		if (l == -1 || l >= (int)sizeof(rcspath)) {
			errno = ENAMETOOLONG;
			cvs_log(LP_ERRNO, "%s", rcspath);
			return (-1);
		}

		rf = rcs_open(rcspath, RCS_READ);
		if (rf == NULL) {
			return (CVS_EX_DATA);
		}

		rcs_close(rf);
	}

	return (ret);
}
