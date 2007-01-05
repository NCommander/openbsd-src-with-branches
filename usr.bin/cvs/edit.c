/*	$OpenBSD: edit.c,v 1.17 2007/01/05 07:13:49 xsa Exp $	*/
/*
 * Copyright (c) 2006, 2007 Xavier Santolaria <xsa@openbsd.org>
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

#include "includes.h"

#include "cvs.h"
#include "log.h"
#include "remote.h"

#define E_COMMIT	0x01
#define E_EDIT		0x02
#define E_UNEDIT	0x04
#define E_ALL		(E_EDIT|E_COMMIT|E_UNEDIT)

static void	cvs_edit_local(struct cvs_file *);
static void	cvs_editors_local(struct cvs_file *);
static void	cvs_unedit_local(struct cvs_file *);

static int	edit_aflags = 0;

struct cvs_cmd cvs_cmd_edit = {
	CVS_OP_EDIT, 0, "edit",
	{ },
	"Get ready to edit a watched file",
	"[-lR] [-a action] [file ...]",
	"a:lR",
	NULL,
	cvs_edit
};

struct cvs_cmd cvs_cmd_editors = {
	CVS_OP_EDITORS, 0, "editors",
	{ },
	"See who is editing a watched file",
	"[-lR] [file ...]",
	"lR",
	NULL,
	cvs_editors
};

struct cvs_cmd cvs_cmd_unedit = {
	CVS_OP_UNEDIT, 0, "unedit",
	{ },
	"Undo an edit command",
	"[-lR] [file ...]",
	"lR",
	NULL,
	cvs_unedit
};

int
cvs_edit(int argc, char **argv)
{
	int ch;
	int flags;
	struct cvs_recursion cr;

	flags = CR_RECURSE_DIRS;

	while ((ch = getopt(argc, argv, cvs_cmd_edit.cmd_opts)) != -1) {
		switch (ch) {
		case 'a':
			if (strcmp(optarg, "edit") == 0)
				edit_aflags |= E_EDIT;
			else if (strcmp(optarg, "unedit") == 0)
				edit_aflags |= E_UNEDIT;
			else if (strcmp(optarg, "commit") == 0)
				edit_aflags |= E_COMMIT;
			else if (strcmp(optarg, "all") == 0)
				edit_aflags |= E_ALL;
			else if (strcmp(optarg, "none") == 0)
				edit_aflags &= ~E_ALL;
			else
				fatal("%s", cvs_cmd_edit.cmd_synopsis);
			break;
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'R':
			break;
		default:
			fatal("%s", cvs_cmd_edit.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		fatal("%s", cvs_cmd_edit.cmd_synopsis);

	if (edit_aflags == 0)
		edit_aflags |= E_ALL;

	cr.enterdir = NULL;
	cr.leavedir = NULL;

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cr.fileproc = cvs_client_sendfile;

		if (!(flags & CR_RECURSE_DIRS))
			cvs_client_send_request("Argument -l");
	} else {
		cr.fileproc = cvs_edit_local;
	}

	cr.flags = flags;

	cvs_file_run(argc, argv, &cr);

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");
		cvs_client_send_request("edit");
		cvs_client_get_responses();
	}

	return (0);
}

int
cvs_editors(int argc, char **argv)
{
	int ch;
	int flags;
	struct cvs_recursion cr;

	flags = CR_RECURSE_DIRS;

	while ((ch = getopt(argc, argv, cvs_cmd_editors.cmd_opts)) != -1) {
		switch (ch) {
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'R':
			break;
		default:
			fatal("%s", cvs_cmd_editors.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		fatal("%s", cvs_cmd_editors.cmd_synopsis);

	cr.enterdir = NULL;
	cr.leavedir = NULL;

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cr.fileproc = cvs_client_sendfile;

		if (!(flags & CR_RECURSE_DIRS))
			cvs_client_send_request("Argument -l");
	} else {
		cr.fileproc = cvs_editors_local;
	}

	cr.flags = flags;

	cvs_file_run(argc, argv, &cr);

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");
		cvs_client_send_request("editors");
		cvs_client_get_responses();
	}

	return (0);
}

int
cvs_unedit(int argc, char **argv)
{
	int ch;
	int flags;
	struct cvs_recursion cr;

	flags = CR_RECURSE_DIRS;

	while ((ch = getopt(argc, argv, cvs_cmd_unedit.cmd_opts)) != -1) {
		switch (ch) {
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'R':
			break;
		default:
			fatal("%s", cvs_cmd_unedit.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		fatal("%s", cvs_cmd_unedit.cmd_synopsis);

	cr.enterdir = NULL;
	cr.leavedir = NULL;

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cr.fileproc = cvs_client_sendfile;

		if (!(flags & CR_RECURSE_DIRS))
			cvs_client_send_request("Argument -l");
	} else {
		cr.fileproc = cvs_unedit_local;
	}

	cr.flags = flags;

	cvs_file_run(argc, argv, &cr);

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");
		cvs_client_send_request("unedit");
		cvs_client_get_responses();
	}

	return (0);
}

static void
cvs_edit_local(struct cvs_file *cf)
{
	FILE *fp;
	struct tm *t;
	time_t now;
	char *bfpath, *fdate;

	if (cvs_noexec == 1)
		return;

	if ((fp = fopen(CVS_PATH_NOTIFY, "a")) == NULL)
		fatal("cvs_edit_local: fopen: `%s': %s",
		    CVS_PATH_NOTIFY, strerror(errno));

	(void)time(&now);
	if ((t = gmtime(&now)) == NULL)
		fatal("gmtime failed");

	fdate = asctime(t);

	(void)fprintf(fp, "E%s\t%s GMT\t%s\t%s\t\n",
	    cf->file_name, fdate, current_cvsroot->cr_host, cf->file_wd);

	if (edit_aflags & E_EDIT)
		(void)fprintf(fp, "E");
	if (edit_aflags & E_UNEDIT)
		(void)fprintf(fp, "U");
	if (edit_aflags & E_COMMIT)
		(void)fprintf(fp, "C");

	(void)fprintf(fp, "\n");

	(void)fclose(fp);

	if (fchmod(cf->fd, 0644) == -1)
		fatal("cvs_edit_local: fchmod %s", strerror(errno));

	bfpath = xmalloc(MAXPATHLEN);
	if (cvs_path_cat(CVS_PATH_BASEDIR, cf->file_name, bfpath,
	    MAXPATHLEN) >= MAXPATHLEN)
		fatal("cvs_edit_local: truncation");

	/* XXX: copy cf->file_path to bfpath */

	xfree(bfpath);

	/* XXX: Update revision number in CVS/Baserev from CVS/Entries */
}

static void
cvs_editors_local(struct cvs_file *cf)
{
}

static void
cvs_unedit_local(struct cvs_file *cf)
{
	FILE *fp;
	struct stat st;
	struct tm *t;
	time_t now;
	char *bfpath, *fdate;

	if (cvs_noexec == 1)
		return;

	bfpath = xmalloc(MAXPATHLEN);
	if (cvs_path_cat(CVS_PATH_BASEDIR, cf->file_name, bfpath,
	    MAXPATHLEN) >= MAXPATHLEN)
		fatal("cvs_unedit_local: truncation");

	if (stat(bfpath, &st) == -1) {
		xfree(bfpath);
		return;
	}

	if (cvs_file_cmp(cf->file_path, bfpath) != 0) {
		cvs_printf("%s has been modified; revert changes? ",
		    cf->file_name);

		if (cvs_yesno() == -1) {
			xfree(bfpath);
			return;
		}
	}

	cvs_rename(bfpath, cf->file_path);
	xfree(bfpath);

	if ((fp = fopen(CVS_PATH_NOTIFY, "a")) == NULL)
		fatal("cvs_unedit_local: fopen: `%s': %s",
		    CVS_PATH_NOTIFY, strerror(errno));

	(void)time(&now);
	if ((t = gmtime(&now)) == NULL)
		fatal("gmtime failed");

	fdate = asctime(t);

	(void)fprintf(fp, "U%s\t%s GMT\t%s\t%s\t\n",
	    cf->file_name, fdate, current_cvsroot->cr_host, cf->file_wd);

	(void)fclose(fp);

	/* XXX: Update revision number in CVS/Entries from CVS/Baserev */

	if (fchmod(cf->fd, 0644) == -1)
		fatal("cvs_unedit_local: fchmod %s", strerror(errno));
}
