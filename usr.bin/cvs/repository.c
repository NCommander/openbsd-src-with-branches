/*	$OpenBSD: repository.c,v 1.7 2006/12/11 07:59:18 xsa Exp $	*/
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

#include "includes.h"

#include "cvs.h"
#include "file.h"
#include "log.h"
#include "repository.h"
#include "worklist.h"

struct cvs_wklhead repo_locks;

void
cvs_repository_unlock(const char *repo)
{
	char fpath[MAXPATHLEN];

	cvs_log(LP_TRACE, "cvs_repository_unlock(%s)", repo);

	if (cvs_path_cat(repo, CVS_LOCK, fpath, sizeof(fpath)) >= sizeof(fpath))
		fatal("cvs_repository_unlock: truncation");

	/* XXX - this ok? */
	cvs_worklist_run(&repo_locks, cvs_worklist_unlink);
}

void
cvs_repository_lock(const char *repo)
{
	int i;
	struct stat st;
	char fpath[MAXPATHLEN];
	struct passwd *pw;

	if (cvs_noexec == 1 || cvs_readonlyfs == 1)
		return;

	cvs_log(LP_TRACE, "cvs_repository_lock(%s)", repo);

	if (cvs_path_cat(repo, CVS_LOCK, fpath, sizeof(fpath)) >= sizeof(fpath))
		fatal("cvs_repository_unlock: truncation");

	for (i = 0; i < CVS_LOCK_TRIES; i++) {
		if (cvs_quit)
			fatal("received signal %d", sig_received);

		if (stat(fpath, &st) == -1)
			break;

		if ((pw = getpwuid(st.st_uid)) == NULL)
			fatal("cvs_repository_lock: %s", strerror(errno));

		cvs_log(LP_NOTICE, "waiting for %s's lock in '%s'",
		    pw->pw_name, repo);
		sleep(CVS_LOCK_SLEEP);
	}

	if (i == CVS_LOCK_TRIES)
		fatal("maximum wait time for lock inside '%s' reached", repo);

	if ((i = open(fpath, O_WRONLY|O_CREAT|O_TRUNC, 0755)) < 0) {
		if (errno == EEXIST)
			fatal("cvs_repository_lock: somebody beat us");
		else
			fatal("cvs_repository_lock: %s: %s",
			    fpath, strerror(errno));
	}

	(void)close(i);
	cvs_worklist_add(fpath, &repo_locks);
}

void
cvs_repository_getdir(const char *dir, const char *wdir,
	struct cvs_flisthead *fl, struct cvs_flisthead *dl, int dodirs)
{
	int type;
	DIR *dirp;
	struct stat st;
	struct dirent *dp;
	char *s, *fpath, *rpath;

	rpath = xmalloc(MAXPATHLEN);
	fpath = xmalloc(MAXPATHLEN);

	if ((dirp = opendir(dir)) == NULL)
		fatal("cvs_repository_getdir: failed to open '%s'", dir);

	while ((dp = readdir(dirp)) != NULL) {
		if (!strcmp(dp->d_name, ".") ||
		    !strcmp(dp->d_name, "..") ||
		    !strcmp(dp->d_name, CVS_PATH_ATTIC) ||
		    !strcmp(dp->d_name, CVS_LOCK))
			continue;

		if (cvs_file_chkign(dp->d_name))
			continue;

		if (cvs_path_cat(wdir, dp->d_name,
		    fpath, MAXPATHLEN) >= MAXPATHLEN)
			fatal("cvs_repository_getdir: truncation");

		if (cvs_path_cat(dir, dp->d_name,
		    rpath, MAXPATHLEN) >= MAXPATHLEN)
			fatal("cvs_repository_getdir: truncation");

		/*
		 * nfs and afs will show d_type as DT_UNKNOWN
		 * for files and/or directories so when we encounter
		 * this we call stat() on the path to be sure.
		 */
		if (dp->d_type == DT_UNKNOWN) {
			if (stat(rpath, &st) == -1)
				fatal("'%s': %s", rpath, strerror(errno));

			switch (st.st_mode & S_IFMT) {
			case S_IFDIR:
				type = CVS_DIR;
				break;
			case S_IFREG:
				type = CVS_FILE;
				break;
			default:
				fatal("Unknown file type in repository");
			}
		} else {
			switch (dp->d_type) {
			case DT_DIR:
				type = CVS_DIR;
				break;
			case DT_REG:
				type = CVS_FILE;
				break;
			default:
				fatal("Unknown file type in repository");
			}
		}

		if (dodirs == 0 && type == CVS_DIR)
			continue;

		switch (type) {
		case CVS_DIR:
			cvs_file_get(fpath, dl);
			break;
		case CVS_FILE:
			if ((s = strrchr(fpath, ',')) != NULL)
				*s = '\0';
			cvs_file_get(fpath, fl);
			break;
		default:
			fatal("type %d unknown, shouldn't happen", type);
		}
	}

	xfree(rpath);
	xfree(fpath);

	(void)closedir(dirp);
}
