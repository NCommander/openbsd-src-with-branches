/*	$OpenBSD: annotate.c,v 1.16 2005/05/30 08:27:03 xsa Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "log.h"
#include "proto.h"


static int cvs_annotate_file      (CVSFILE *, void *);
static int cvs_annotate_options   (struct cvs_cmd *, int, char **, int *);
static int cvs_annotate_sendflags (struct cvsroot *);


struct cvs_cmd cvs_cmd_annotate = {
	CVS_OP_ANNOTATE, CVS_REQ_ANNOTATE, "annotate",
	{ "ann", "blame"  },
	"Show last revision where each line was modified",
	"[-flR] [-D date | -r rev] ...",
	"D:flRr:",
	NULL,
	CF_SORT | CF_RECURSE | CF_IGNORE | CF_NOSYMS,
	cvs_annotate_options,
	cvs_annotate_sendflags,
	cvs_annotate_file,
	cvs_annotate_file,
	NULL,
	NULL,
	CVS_CMD_ALLOWSPEC | CVS_CMD_SENDDIR | CVS_CMD_SENDARGS2
};	

static char *date, *rev;
static int usehead;

static int
cvs_annotate_options(struct cvs_cmd *cmd, int argc, char **argv, int *arg)
{
	int ch;

	usehead = 0;
	date = NULL;
	rev = NULL;

	while ((ch = getopt(argc, argv, cmd->cmd_opts)) != -1) {
		switch (ch) {
		case 'D':
			date = optarg;
			break;
		case 'f':
			usehead = 1;
			break;
		case 'l':
			cmd->file_flags &= ~CF_RECURSE;
			break;
		case 'R':
			cmd->file_flags |= CF_RECURSE;
			break;
		case 'r':
			rev = optarg;
			break;
		default:
			return (CVS_EX_USAGE);
		}
	}

	*arg = optind;
	return (0);
}

static int
cvs_annotate_sendflags(struct cvsroot *root)
{
	if (usehead && (cvs_sendarg(root, "-f", 0) < 0))
		return (CVS_EX_PROTO);

	if (rev != NULL) {
		if ((cvs_sendarg(root, "-r", 0) < 0) ||
		    (cvs_sendarg(root, rev, 0) < 0))
			return (CVS_EX_PROTO);
	}

	if (date != NULL) {
		if ((cvs_sendarg(root, "-D", 0) < 0) ||
		    (cvs_sendarg(root, date, 0) < 0))
			return (CVS_EX_PROTO);
	}

	return (0);
}

/*
 * cvs_annotate_file()
 *
 * Annotate a single file.
 */
static int
cvs_annotate_file(CVSFILE *cf, void *arg)
{
	int ret;
	char fpath[MAXPATHLEN];
	struct cvsroot *root;

	ret = 0;
	root = CVS_DIR_ROOT(cf);

	if (cf->cf_type == DT_DIR) {
		if (cf->cf_cvstat == CVS_FST_UNKNOWN)
			ret = cvs_sendreq(root, CVS_REQ_QUESTIONABLE,
			    cf->cf_name);
		else
			ret = cvs_senddir(root, cf);

		if (ret == -1)
			ret = CVS_EX_PROTO;

		return (ret);
	}

	cvs_file_getpath(cf, fpath, sizeof(fpath));

	if (cvs_sendentry(root, cf) < 0) {
		return (CVS_EX_PROTO);
	}

	switch (cf->cf_cvstat) {
	case CVS_FST_UNKNOWN:
		ret = cvs_sendreq(root, CVS_REQ_QUESTIONABLE, cf->cf_name);
		break;
	case CVS_FST_UPTODATE:
		ret = cvs_sendreq(root, CVS_REQ_UNCHANGED, cf->cf_name);
		break;
	case CVS_FST_ADDED:
	case CVS_FST_MODIFIED:
		ret = cvs_sendreq(root, CVS_REQ_ISMODIFIED, cf->cf_name);
		break;
	default:
		break;
	}

	if (ret == -1)
		ret = CVS_EX_PROTO;

	return (ret);
}
