/*	$OpenBSD: getlog.c,v 1.15 2005/03/26 08:09:54 tedu Exp $	*/
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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <paths.h>
#include <sysexits.h>

#include "cvs.h"
#include "log.h"
#include "file.h"
#include "proto.h"


#define CVS_GLOG_RFONLY    0x01
#define CVS_GLOG_HDONLY    0x02


#define CVS_GETLOG_REVSEP   "----------------------------"
#define CVS_GETLOG_REVEND \
 "============================================================================="

#ifdef notyet
static void cvs_getlog_print   (const char *, RCSFILE *, u_int);
#endif
static int  cvs_getlog_file    (CVSFILE *, void *);

int cvs_getlog_options(char *, int, char **, int *);

struct cvs_cmd_info cvs_getlog = {
	cvs_getlog_options,
	NULL,
	cvs_getlog_file,
	NULL, NULL,
	CF_RECURSE,
	CVS_REQ_LOG,
	CVS_CMD_SENDDIR | CVS_CMD_ALLOWSPEC | CVS_CMD_SENDARGS2
};

static int rfonly = 0;
static int honly = 0;

int
cvs_getlog_options(char *opt, int argc, char **argv, int *arg)
{
	int ch;

	while ((ch = getopt(argc, argv, opt)) != -1) {
		switch (ch) {
		case 'd':
			break;
		case 'h':
			honly = 1;
			break;
		case 'l':
			cvs_getlog.file_flags &= ~CF_RECURSE;
			break;
		case 'R':
			rfonly = 1;
			break;
		case 'r':
			break;
		default:
			return (EX_USAGE);
		}
	}

	*arg = optind;
	return (0);
}


/*
 * cvs_getlog_file()
 *
 * Diff a single file.
 */
static int
cvs_getlog_file(CVSFILE *cf, void *arg)
{
	int ret;
	char *repo, fpath[MAXPATHLEN];
	RCSFILE *rf;
	struct cvsroot *root;
	struct cvs_ent *entp;

	ret = 0;
	rf = NULL;
	root = CVS_DIR_ROOT(cf);
	repo = CVS_DIR_REPO(cf);

	if (cf->cf_type == DT_DIR) {
		if (root->cr_method != CVS_METHOD_LOCAL) {
			if (cf->cf_cvstat == CVS_FST_UNKNOWN)
				ret = cvs_sendreq(root, CVS_REQ_QUESTIONABLE,
				    CVS_FILE_NAME(cf));
			else
				ret = cvs_senddir(root, cf);
		}

		return (ret);
	}

	cvs_file_getpath(cf, fpath, sizeof(fpath));
	entp = cvs_ent_getent(fpath);

	if (root->cr_method != CVS_METHOD_LOCAL) {
		if ((entp != NULL) && (cvs_sendentry(root, entp) < 0)) {
			cvs_ent_free(entp);
			return (-1);
		}

		switch (cf->cf_cvstat) {
		case CVS_FST_UNKNOWN:
			ret = cvs_sendreq(root, CVS_REQ_QUESTIONABLE,
			    CVS_FILE_NAME(cf));
			break;
		case CVS_FST_UPTODATE:
			ret = cvs_sendreq(root, CVS_REQ_UNCHANGED,
			    CVS_FILE_NAME(cf));
			break;
		case CVS_FST_ADDED:
		case CVS_FST_MODIFIED:
			ret = cvs_sendreq(root, CVS_REQ_ISMODIFIED,
			    CVS_FILE_NAME(cf));
			break;
		default:
			break;
		}
	} else {
		if (cf->cf_cvstat == CVS_FST_UNKNOWN) {
			cvs_printf("? %s\n", fpath);
			return (0);
		}

		snprintf(fpath, sizeof(fpath), "%s/%s/%s%s",
		    root->cr_dir, repo, CVS_FILE_NAME(cf), RCS_FILE_EXT);

		rf = rcs_open(fpath, RCS_READ);
		if (rf == NULL) {
			if (entp != NULL)
				cvs_ent_free(entp);
			return (-1);
		}

		rcs_close(rf);
	}

	if (entp != NULL)
		cvs_ent_free(entp);
	return (ret);
}

#ifdef notyet
static void
cvs_getlog_print(const char *file, RCSFILE *rfp, u_int flags)
{
	char numbuf[64], datebuf[64], *sp;
	struct rcs_delta *rdp;

	cvs_printf("RCS file: %s\nWorking file: %s\n",
	    rfp->rf_path, file);
	cvs_printf("Working file: %s\n", (char *)NULL);
	cvs_printf("head: %s\nbranch:\nlocks:\naccess list:\n");
	cvs_printf("symbolic names:\nkeyword substitutions:\n");
	cvs_printf("total revisions: %u;\tselected revisions: %u\n", 1, 1);

	cvs_printf("description:\n");

	for (;;) {
		cvs_printf(CVS_GETLOG_REVSEP "\n");
		rcsnum_tostr(rdp->rd_num, numbuf, sizeof(numbuf));
		cvs_printf("revision %s\n", numbuf);
		cvs_printf("date: %d/%02d/%d %02d:%02d:%02d;  author: %s;"
		    "  state: %s;  lines:",
		    rdp->rd_date.tm_year, rdp->rd_date.tm_mon + 1,
		    rdp->rd_date.tm_mday, rdp->rd_date.tm_hour,
		    rdp->rd_date.tm_min, rdp->rd_date.tm_sec,
		    rdp->rd_author, rdp->rd_state);
	}

	cvs_printf(CVS_GETLOG_REVEND "\n");
}
#endif
