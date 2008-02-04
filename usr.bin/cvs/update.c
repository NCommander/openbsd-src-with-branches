/*	$OpenBSD: update.c,v 1.118 2008/02/03 15:20:10 tobias Exp $	*/
/*
 * Copyright (c) 2006 Joris Vink <joris@openbsd.org>
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

#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "diff.h"
#include "remote.h"

int	prune_dirs = 0;
int	print_stdout = 0;
int	build_dirs = 0;
int	reset_stickies = 0;
char *cvs_specified_tag = NULL;

static char *koptstr;

static void update_clear_conflict(struct cvs_file *);

struct cvs_cmd cvs_cmd_update = {
	CVS_OP_UPDATE, CVS_USE_WDIR, "update",
	{ "up", "upd" },
	"Bring work tree in sync with repository",
	"[-ACdflPpR] [-D date | -r rev] [-I ign] [-j rev] [-k mode] "
	"[-t id] ...",
	"ACD:dfI:j:k:lPpQqRr:t:u",
	NULL,
	cvs_update
};

int
cvs_update(int argc, char **argv)
{
	int ch;
	char *arg = ".";
	int flags;
	struct cvs_recursion cr;

	flags = CR_RECURSE_DIRS;

	while ((ch = getopt(argc, argv, cvs_cmd_update.cmd_opts)) != -1) {
		switch (ch) {
		case 'A':
			reset_stickies = 1;
			break;
		case 'C':
		case 'D':
			cvs_specified_tag = optarg;
			break;
		case 'd':
			build_dirs = 1;
			break;
		case 'f':
			break;
		case 'I':
			break;
		case 'j':
			break;
		case 'k':
			koptstr = optarg;
			kflag = rcs_kflag_get(koptstr);
			if (RCS_KWEXP_INVAL(kflag)) {
				cvs_log(LP_ERR,
				    "invalid RCS keyword expension mode");
				fatal("%s", cvs_cmd_add.cmd_synopsis);
			}
			break;
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'P':
			prune_dirs = 1;
			break;
		case 'p':
			print_stdout = 1;
			cvs_noexec = 1;
			break;
		case 'Q':
		case 'q':
			break;
		case 'R':
			flags |= CR_RECURSE_DIRS;
			break;
		case 'r':
			cvs_specified_tag = optarg;
			break;
		case 'u':
			break;
		default:
			fatal("%s", cvs_cmd_update.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	if (current_cvsroot->cr_method == CVS_METHOD_LOCAL) {
		cr.enterdir = cvs_update_enterdir;
		cr.leavedir = prune_dirs ? cvs_update_leavedir : NULL;
		cr.fileproc = cvs_update_local;
		flags |= CR_REPO;
	} else {
		cvs_client_connect_to_server();
		if (reset_stickies)
			cvs_client_send_request("Argument -A");
		if (build_dirs)
			cvs_client_send_request("Argument -d");
		if (kflag)
			cvs_client_send_request("Argument -k%s", koptstr);
		if (!(flags & CR_RECURSE_DIRS))
			cvs_client_send_request("Argument -l");
		if (prune_dirs)
			cvs_client_send_request("Argument -P");
		if (print_stdout)
			cvs_client_send_request("Argument -p");

		if (cvs_specified_tag != NULL)
			cvs_client_send_request("Argument -r%s",
			    cvs_specified_tag);

		cr.enterdir = NULL;
		cr.leavedir = NULL;
		cr.fileproc = cvs_client_sendfile;
	}

	cr.flags = flags;

	if (argc > 0)
		cvs_file_run(argc, argv, &cr);
	else
		cvs_file_run(1, &arg, &cr);

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");
		cvs_client_send_request("update");
		cvs_client_get_responses();
	}

	return (0);
}

void
cvs_update_enterdir(struct cvs_file *cf)
{
	CVSENTRIES *entlist;
	char *entry, fpath[MAXPATHLEN];

	cvs_log(LP_TRACE, "cvs_update_enterdir(%s)", cf->file_path);

	cvs_file_classify(cf, cvs_directory_tag);

	if (cf->file_status == DIR_CREATE && build_dirs == 1) {
		cvs_mkpath(cf->file_path, cvs_specified_tag);
		if ((cf->fd = open(cf->file_path, O_RDONLY)) == -1)
			fatal("cvs_update_enterdir: `%s': %s",
			    cf->file_path, strerror(errno));

		if (cvs_cmdop != CVS_OP_EXPORT) {
			(void)xasprintf(&entry, "D/%s////", cf->file_name);

			entlist = cvs_ent_open(cf->file_wd);
			cvs_ent_add(entlist, entry);
			cvs_ent_close(entlist, ENT_SYNC);
			xfree(entry);
		}
	} else if ((cf->file_status == DIR_CREATE && build_dirs == 0) ||
		    cf->file_status == FILE_UNKNOWN) {
		cf->file_status = FILE_SKIP;
	} else if (reset_stickies == 1) {
		(void)xsnprintf(fpath, MAXPATHLEN, "%s/%s",
		    cf->file_path, CVS_PATH_TAG);
		(void)unlink(fpath);
	} else {
		if (cvs_specified_tag != NULL)
			cvs_write_tagfile(cf->file_path,
				    cvs_specified_tag, NULL, 0);
	}
}

void
cvs_update_leavedir(struct cvs_file *cf)
{
	long base;
	int nbytes;
	int isempty;
	size_t bufsize;
	struct stat st;
	struct dirent *dp;
	char *buf, *ebuf, *cp;
	struct cvs_ent *ent;
	struct cvs_ent_line *line;
	CVSENTRIES *entlist;

	cvs_log(LP_TRACE, "cvs_update_leavedir(%s)", cf->file_path);

	if (cvs_server_active == 1 && !strcmp(cf->file_name, "."))
		return;

	if (fstat(cf->fd, &st) == -1)
		fatal("cvs_update_leavedir: %s", strerror(errno));

	bufsize = st.st_size;
	if (bufsize < st.st_blksize)
		bufsize = st.st_blksize;

	isempty = 1;
	buf = xmalloc(bufsize);

	if (lseek(cf->fd, 0, SEEK_SET) == -1)
		fatal("cvs_update_leavedir: %s", strerror(errno));

	while ((nbytes = getdirentries(cf->fd, buf, bufsize, &base)) > 0) {
		ebuf = buf + nbytes;
		cp = buf;

		while (cp < ebuf) {
			dp = (struct dirent *)cp;
			if (!strcmp(dp->d_name, ".") ||
			    !strcmp(dp->d_name, "..") ||
			    dp->d_fileno == 0) {
				cp += dp->d_reclen;
				continue;
			}

			if (!strcmp(dp->d_name, CVS_PATH_CVSDIR)) {
				entlist = cvs_ent_open(cf->file_path);
				TAILQ_FOREACH(line, &(entlist->cef_ent),
				    entries_list) {
					ent = cvs_ent_parse(line->buf);

					if (ent->ce_status == CVS_ENT_REMOVED) {
						isempty = 0;
						cvs_ent_free(ent);
						break;
					}

					cvs_ent_free(ent);
				}
				cvs_ent_close(entlist, ENT_NOSYNC);
			} else {
				isempty = 0;
			}

			if (isempty == 0)
				break;

			cp += dp->d_reclen;
		}
	}

	if (nbytes == -1)
		fatal("cvs_update_leavedir: %s", strerror(errno));

	xfree(buf);

	if (isempty == 1 && prune_dirs == 1) {
		/* XXX */
		cvs_rmdir(cf->file_path);

		if (cvs_server_active == 0 && cvs_cmdop != CVS_OP_EXPORT) {
			entlist = cvs_ent_open(cf->file_wd);
			cvs_ent_remove(entlist, cf->file_name);
			cvs_ent_close(entlist, ENT_SYNC);
		}
	}
}

void
cvs_update_local(struct cvs_file *cf)
{
	char *tag;
	int ret, flags;
	CVSENTRIES *entlist;
	char kbuf[8], rbuf[CVS_REV_BUFSZ];

	cvs_log(LP_TRACE, "cvs_update_local(%s)", cf->file_path);

	if (cf->file_type == CVS_DIR) {
		if (cf->file_status == FILE_SKIP) {
			if (cvs_cmdop == CVS_OP_EXPORT && verbosity > 0)
				cvs_printf("? %s\n", cf->file_path);
			return;
		}

		if (cf->file_status != FILE_UNKNOWN &&
		    verbosity > 1)
			cvs_log(LP_NOTICE, "Updating %s", cf->file_path);
		return;
	}

	flags = 0;
	if (cvs_specified_tag != NULL)
		tag = cvs_specified_tag;
	else
		tag = cvs_directory_tag;

	cvs_file_classify(cf, tag);

	if (kflag)
		rcs_kwexp_set(cf->file_rcs, kflag);

	if (cf->file_ent != NULL && cf->file_ent->ce_tag != NULL)
		tag = cf->file_ent->ce_tag;

	if ((cf->file_status == FILE_UPTODATE ||
	    cf->file_status == FILE_MODIFIED) && cf->file_ent != NULL &&
	    cf->file_ent->ce_tag != NULL && reset_stickies == 1) {
		if (cf->file_status == FILE_MODIFIED)
			cf->file_status = FILE_MERGE;
		else
			cf->file_status = FILE_CHECKOUT;

		cf->file_rcsrev = rcs_head_get(cf->file_rcs);

		/* might be a bit overkill */
		if (cvs_server_active == 1)
			cvs_server_clear_sticky(cf->file_wd);
	}

	if (print_stdout && cf->file_status != FILE_UNKNOWN &&
	    !cf->file_rcs->rf_dead) {
		rcsnum_tostr(cf->file_rcsrev, rbuf, sizeof(rbuf));
		if (verbosity > 1) {
			cvs_log(LP_RCS, RCS_DIFF_DIV);
			cvs_log(LP_RCS, "Checking out %s", cf->file_path);
			cvs_log(LP_RCS, "RCS:  %s", cf->file_rpath);
			cvs_log(LP_RCS, "VERS: %s", rbuf);
			cvs_log(LP_RCS, "***************");
		}
		cvs_checkout_file(cf, cf->file_rcsrev, tag, CO_DUMP);
		return;
	}

	if (cf->file_ent != NULL) {
		if (cf->file_ent->ce_opts == NULL) {
			if (kflag)
				cf->file_status = FILE_CHECKOUT;
		} else {
			if (kflag) {
				(void)xsnprintf(kbuf, sizeof(kbuf),
				    "-k%s", cf->file_rcs->rf_expand);

			    	if (strcmp(kbuf, cf->file_ent->ce_opts))
					cf->file_status = FILE_CHECKOUT;
			} else if (reset_stickies)
				cf->file_status = FILE_CHECKOUT;
		}
	}

	switch (cf->file_status) {
	case FILE_UNKNOWN:
		cvs_printf("? %s\n", cf->file_path);
		break;
	case FILE_MODIFIED:
		ret = update_has_conflict_markers(cf);
		if (cf->file_ent->ce_conflict != NULL && ret == 1) {
			cvs_printf("C %s\n", cf->file_path);
		} else {
			if (cf->file_ent->ce_conflict != NULL && ret == 0)
				update_clear_conflict(cf);
			cvs_printf("M %s\n", cf->file_path);
		}
		break;
	case FILE_ADDED:
		cvs_printf("A %s\n", cf->file_path);
		break;
	case FILE_REMOVED:
		cvs_printf("R %s\n", cf->file_path);
		break;
	case FILE_CONFLICT:
		cvs_printf("C %s\n", cf->file_path);
		break;
	case FILE_LOST:
	case FILE_CHECKOUT:
	case FILE_PATCH:
		if ((tag != NULL && reset_stickies != 1) ||
		    (((cf->file_ent != NULL) && cf->file_ent->ce_tag != NULL) &&
		    (reset_stickies != 1)))
			flags = CO_SETSTICKY;

		cvs_checkout_file(cf, cf->file_rcsrev, tag, flags);
		cvs_printf("U %s\n", cf->file_path);
		cvs_history_add(CVS_HISTORY_UPDATE_CO, cf, NULL);
		break;
	case FILE_MERGE:
		cvs_checkout_file(cf, cf->file_rcsrev, tag, CO_MERGE);

		if (diff3_conflicts != 0) {
			cvs_printf("C %s\n", cf->file_path);
			cvs_history_add(CVS_HISTORY_UPDATE_MERGED_ERR,
			    cf, NULL);
		} else {
			update_clear_conflict(cf);
			cvs_printf("M %s\n", cf->file_path);
			cvs_history_add(CVS_HISTORY_UPDATE_MERGED, cf, NULL);
		}
		break;
	case FILE_UNLINK:
		(void)unlink(cf->file_path);
		if (cvs_server_active == 1)
			cvs_checkout_file(cf, cf->file_rcsrev, tag, CO_REMOVE);
	case FILE_REMOVE_ENTRY:
		entlist = cvs_ent_open(cf->file_wd);
		cvs_ent_remove(entlist, cf->file_name);
		cvs_ent_close(entlist, ENT_SYNC);
		cvs_history_add(CVS_HISTORY_UPDATE_REMOVE, cf, NULL);
		break;
	case FILE_UPTODATE:
		if (cvs_cmdop != CVS_OP_UPDATE)
			break;

		if (tag != NULL && cf->file_rcs->rf_dead != 1 &&
		    (cf->file_flags & FILE_HAS_TAG))
			cvs_checkout_file(cf, cf->file_rcsrev,
			    tag, CO_SETSTICKY);
		break;
	default:
		break;
	}
}

static void
update_clear_conflict(struct cvs_file *cf)
{
	time_t now;
	CVSENTRIES *entlist;
	char *entry, revbuf[CVS_REV_BUFSZ], timebuf[CVS_TIME_BUFSZ];

	cvs_log(LP_TRACE, "update_clear_conflict(%s)", cf->file_path);

	time(&now);
	ctime_r(&now, timebuf);
	timebuf[strcspn(timebuf, "\n")] = '\0';

	rcsnum_tostr(cf->file_ent->ce_rev, revbuf, sizeof(revbuf));

	entry = xmalloc(CVS_ENT_MAXLINELEN);
	(void)xsnprintf(entry, CVS_ENT_MAXLINELEN, "/%s/%s/%s/%s/%s",
	    cf->file_name, revbuf, timebuf, cf->file_ent->ce_opts ? : "",
	    cf->file_ent->ce_tag ? : "");

	entlist = cvs_ent_open(cf->file_wd);
	cvs_ent_add(entlist, entry);
	cvs_ent_close(entlist, ENT_SYNC);
	xfree(entry);
}

/*
 * XXX - this is the way GNU cvs checks for outstanding conflicts
 * in a file after a merge. It is a very very bad approach and
 * should be looked at once opencvs is working decently.
 */
int
update_has_conflict_markers(struct cvs_file *cf)
{
	BUF *bp;
	int conflict;
	char *content;
	struct cvs_line *lp;
	struct cvs_lines *lines;
	size_t len;

	cvs_log(LP_TRACE, "update_has_conflict_markers(%s)", cf->file_path);

	if (cf->fd == -1)
		return (0);

	if ((bp = cvs_buf_load_fd(cf->fd, BUF_AUTOEXT)) == NULL)
		fatal("update_has_conflict_markers: failed to load %s",
		    cf->file_path);

	cvs_buf_putc(bp, '\0');
	len = cvs_buf_len(bp);
	content = cvs_buf_release(bp);
	if ((lines = cvs_splitlines(content, len)) == NULL)
		fatal("update_has_conflict_markers: failed to split lines");

	conflict = 0;
	TAILQ_FOREACH(lp, &(lines->l_lines), l_list) {
		if (lp->l_line == NULL)
			continue;

		if (!strncmp(lp->l_line, RCS_CONFLICT_MARKER1,
		    sizeof(RCS_CONFLICT_MARKER1) - 1) ||
		    !strncmp(lp->l_line, RCS_CONFLICT_MARKER2,
		    sizeof(RCS_CONFLICT_MARKER2) - 1) ||
		    !strncmp(lp->l_line, RCS_CONFLICT_MARKER3,
		    sizeof(RCS_CONFLICT_MARKER3) - 1)) {
			conflict = 1;
			break;
		}
	}

	cvs_freelines(lines);
	xfree(content);
	return (conflict);
}
