/*	$OpenBSD: file.c,v 1.168 2007/01/12 23:32:01 niallo Exp $	*/
/*
 * Copyright (c) 2006 Joris Vink <joris@openbsd.org>
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

#include "includes.h"

#include <sys/mman.h>

#include "cvs.h"
#include "file.h"
#include "log.h"

#define CVS_IGN_STATIC	0x01	/* pattern is static, no need to glob */

#define CVS_CHAR_ISMETA(c)	((c == '*') || (c == '?') || (c == '['))

/*
 * Standard patterns to ignore.
 */
static const char *cvs_ign_std[] = {
	".",
	"..",
	"*.o",
	"*.a",
	"*.bak",
	"*.orig",
	"*.rej",
	"*.old",
	"*.exe",
	"*.depend",
	"*.obj",
	"*.elc",
	"*.ln",
	"*.olb",
	"CVS",
	"core",
	"cvslog*",
	"*.core",
	".#*",
	"*~",
	"_$*",
	"*$",
};

struct ignore_head cvs_ign_pats;
struct ignore_head dir_ign_pats;

void
cvs_file_init(void)
{
	int i, l;
	FILE *ifp;
	size_t len;
	char *path, *buf;

	path = xmalloc(MAXPATHLEN);
	buf = xmalloc(MAXNAMLEN);

	TAILQ_INIT(&cvs_ign_pats);
	TAILQ_INIT(&dir_ign_pats);

	/* standard patterns to ignore */
	for (i = 0; i < (int)(sizeof(cvs_ign_std)/sizeof(char *)); i++)
		cvs_file_ignore(cvs_ign_std[i], &cvs_ign_pats);

	/* read the cvsignore file in the user's home directory, if any */
	l = snprintf(path, MAXPATHLEN, "%s/.cvsignore", cvs_homedir);
	if (l == -1 || l >= MAXPATHLEN)
		fatal("overflow in cvs_file_init");

	ifp = fopen(path, "r");
	if (ifp == NULL) {
		if (errno != ENOENT)
			cvs_log(LP_ERRNO,
			    "failed to open user's cvsignore file `%s'", path);
	} else {
		while (fgets(buf, MAXNAMLEN, ifp) != NULL) {
			len = strlen(buf);
			if (len == 0)
				continue;
			if (buf[len - 1] == '\n')
				buf[len - 1] = '\0';

			cvs_file_ignore(buf, &cvs_ign_pats);
		}

		(void)fclose(ifp);
	}

	xfree(path);
	xfree(buf);
}

void
cvs_file_ignore(const char *pat, struct ignore_head *list)
{
	char *cp;
	size_t len;
	struct cvs_ignpat *ip;

	ip = xmalloc(sizeof(*ip));
	len = strlcpy(ip->ip_pat, pat, sizeof(ip->ip_pat));
	if (len >= sizeof(ip->ip_pat))
		fatal("cvs_file_ignore: truncation of pattern '%s'", pat);

	/* check if we will need globbing for that pattern */
	ip->ip_flags = CVS_IGN_STATIC;
	for (cp = ip->ip_pat; *cp != '\0'; cp++) {
		if (CVS_CHAR_ISMETA(*cp)) {
			ip->ip_flags &= ~CVS_IGN_STATIC;
			break;
		}
	}

	TAILQ_INSERT_TAIL(list, ip, ip_list);
}

int
cvs_file_chkign(const char *file)
{
	int flags;
	struct cvs_ignpat *ip;

	flags = FNM_PERIOD;
	if (cvs_nocase)
		flags |= FNM_CASEFOLD;

	TAILQ_FOREACH(ip, &cvs_ign_pats, ip_list) {
		if (ip->ip_flags & CVS_IGN_STATIC) {
			if (cvs_file_cmpname(file, ip->ip_pat) == 0)
				return (1);
		} else if (fnmatch(ip->ip_pat, file, flags) == 0)
			return (1);
	}

	TAILQ_FOREACH(ip, &dir_ign_pats, ip_list) {
		if (ip->ip_flags & CVS_IGN_STATIC) {
			if (cvs_file_cmpname(file, ip->ip_pat) == 0)
				return (1);
		} else if (fnmatch(ip->ip_pat, file, flags) == 0)
			return (1);
	}

	return (0);
}

void
cvs_file_run(int argc, char **argv, struct cvs_recursion *cr)
{
	int i;
	struct cvs_flisthead fl;

	TAILQ_INIT(&fl);

	for (i = 0; i < argc; i++)
		cvs_file_get(argv[i], &fl);

	cvs_file_walklist(&fl, cr);
	cvs_file_freelist(&fl);
}

struct cvs_filelist *
cvs_file_get(const char *name, struct cvs_flisthead *fl)
{
	const char *p;
	struct cvs_filelist *l;

	for (p = name; p[0] == '.' && p[1] == '/';)
		p += 2;

	TAILQ_FOREACH(l, fl, flist)
		if (!strcmp(l->file_path, p))
			return (l);

	l = (struct cvs_filelist *)xmalloc(sizeof(*l));
	l->file_path = xstrdup(p);

	TAILQ_INSERT_TAIL(fl, l, flist);
	return (l);
}

struct cvs_file *
cvs_file_get_cf(const char *d, const char *f, int fd, int type)
{
	int l;
	struct cvs_file *cf;
	char *p, *rpath;

	rpath = xmalloc(MAXPATHLEN);

	l = snprintf(rpath, MAXPATHLEN, "%s/%s", d, f);
	if (l == -1 || l >= MAXPATHLEN)
		fatal("cvs_file_get_cf: overflow");

	for (p = rpath; p[0] == '.' && p[1] == '/';)
		p += 2;

	cf = (struct cvs_file *)xmalloc(sizeof(*cf));
	memset(cf, 0, sizeof(*cf));

	cf->file_name = xstrdup(f);
	cf->file_wd = xstrdup(d);
	cf->file_path = xstrdup(p);
	cf->fd = fd;
	cf->repo_fd = -1;
	cf->file_type = type;
	cf->file_status = cf->file_flags = 0;
	cf->file_ent = NULL;

	xfree(rpath);
	return (cf);
}

void
cvs_file_walklist(struct cvs_flisthead *fl, struct cvs_recursion *cr)
{
	int len, fd, type;
	struct stat st;
	struct cvs_file *cf;
	struct cvs_filelist *l, *nxt;
	char *d, *f, *repo, *fpath;

	fpath = xmalloc(MAXPATHLEN);
	repo = xmalloc(MAXPATHLEN);

	for (l = TAILQ_FIRST(fl); l != NULL; l = nxt) {
		if (cvs_quit)
			fatal("received signal %d", sig_received);

		cvs_log(LP_TRACE, "cvs_file_walklist: element '%s'",
		    l->file_path);

		if ((f = basename(l->file_path)) == NULL)
			fatal("cvs_file_walklist: basename failed");
		if ((d = dirname(l->file_path)) == NULL)
			fatal("cvs_file_walklist: dirname failed");

		type = CVS_FILE;
		if ((fd = open(l->file_path, O_RDONLY)) != -1) {
			if (fstat(fd, &st) == -1) {
				cvs_log(LP_ERRNO, "%s", l->file_path);
				(void)close(fd);
				goto next;
			}

			if (S_ISDIR(st.st_mode))
				type = CVS_DIR;
			else if (S_ISREG(st.st_mode))
				type = CVS_FILE;
			else {
				cvs_log(LP_ERR,
				    "ignoring bad file type for %s",
				    l->file_path);
				(void)close(fd);
				goto next;
			}
		} else if (current_cvsroot->cr_method == CVS_METHOD_LOCAL) {
			if (stat(d, &st) == -1) {
				cvs_log(LP_ERRNO, "%s", d);
				goto next;
			}

			cvs_get_repository_path(d, repo, MAXPATHLEN);
			len = snprintf(fpath, MAXPATHLEN, "%s/%s",
			    repo, f);
			if (len == -1 || len >= MAXPATHLEN)
				fatal("cvs_file_walklist: overflow");

			if ((fd = open(fpath, O_RDONLY)) == -1) {
				strlcat(fpath, RCS_FILE_EXT, MAXPATHLEN);
				fd = open(fpath, O_RDONLY);
			}

			if (fd != -1) {
				if (fstat(fd, &st) == -1)
					fatal("cvs_file_walklist: %s: %s",
					     fpath, strerror(errno));

				if (S_ISDIR(st.st_mode))
					type = CVS_DIR;
				else if (S_ISREG(st.st_mode))
					type = CVS_FILE;
				else {
					cvs_log(LP_ERR,
					    "ignoring bad file type for %s",
					    l->file_path);
					(void)close(fd);
					goto next;
				}
			
				/* this file is not in our working copy yet */
				(void)close(fd);
				fd = -1;
			}
		}

		cf = cvs_file_get_cf(d, f, fd, type);
		if (cf->file_type == CVS_DIR) {
			cvs_file_walkdir(cf, cr);
		} else {
			if (cr->fileproc != NULL)
				cr->fileproc(cf);
		}

		cvs_file_free(cf);

next:
		nxt = TAILQ_NEXT(l, flist);
		TAILQ_REMOVE(fl, l, flist);

		xfree(l->file_path);
		xfree(l);
	}

	xfree(fpath);
	xfree(repo);
}

void
cvs_file_walkdir(struct cvs_file *cf, struct cvs_recursion *cr)
{
	int l;
	FILE *fp;
	int nbytes;
	size_t len;
	long base;
	size_t bufsize;
	struct stat st;
	struct dirent *dp;
	struct cvs_ent *ent;
	struct cvs_ignpat *ip;
	struct cvs_ent_line *line;
	struct cvs_flisthead fl, dl;
	CVSENTRIES *entlist;
	char *buf, *ebuf, *cp, *repo, *fpath;

	cvs_log(LP_TRACE, "cvs_file_walkdir(%s)", cf->file_path);

	if (cr->enterdir != NULL)
		cr->enterdir(cf);

	if (cr->fileproc != NULL)
		cr->fileproc(cf);

	if (cf->file_status == FILE_SKIP)
		return;

	fpath = xmalloc(MAXPATHLEN);

	/*
	 * If we do not have a admin directory inside here, dont bother,
	 * unless we are running import.
	 */
	l = snprintf(fpath, MAXPATHLEN, "%s/%s", cf->file_path,
	    CVS_PATH_CVSDIR);
	if (l == -1 || l >= MAXPATHLEN)
		fatal("cvs_file_walkdir: overflow");

	l = stat(fpath, &st);
	if (cvs_cmdop != CVS_OP_IMPORT &&
	    (l == -1 || (l == 0 && !S_ISDIR(st.st_mode)))) {
		xfree(fpath);
		return;
	}

	/*
	 * check for a local .cvsignore file
	 */
	l = snprintf(fpath, MAXPATHLEN, "%s/.cvsignore", cf->file_path);
	if (l == -1 || l >= MAXPATHLEN)
		fatal("cvs_file_walkdir: overflow");

	if ((fp = fopen(fpath, "r")) != NULL) {
		while (fgets(fpath, MAXPATHLEN, fp) != NULL) {
			len = strlen(fpath);
			if (len == 0)
				continue;
			if (fpath[len - 1] == '\n')
				fpath[len - 1] = '\0';

			cvs_file_ignore(fpath, &dir_ign_pats);
		}

		(void)fclose(fp);
	}

	if (fstat(cf->fd, &st) == -1)
		fatal("cvs_file_walkdir: %s %s", cf->file_path,
		    strerror(errno));

	bufsize = st.st_size;
	if (bufsize < st.st_blksize)
		bufsize = st.st_blksize;

	buf = xmalloc(bufsize);
	TAILQ_INIT(&fl);
	TAILQ_INIT(&dl);

	while ((nbytes = getdirentries(cf->fd, buf, bufsize, &base)) > 0) {
		ebuf = buf + nbytes;
		cp = buf;

		while (cp < ebuf) {
			dp = (struct dirent *)cp;
			if (!strcmp(dp->d_name, ".") ||
			    !strcmp(dp->d_name, "..") ||
			    !strcmp(dp->d_name, CVS_PATH_CVSDIR) ||
			    dp->d_reclen == 0) {
				cp += dp->d_reclen;
				continue;
			}

			if (cvs_file_chkign(dp->d_name)) {
				cp += dp->d_reclen;
				continue;
			}

			if (!(cr->flags & CR_RECURSE_DIRS) &&
			    dp->d_type == DT_DIR) {
				cp += dp->d_reclen;
				continue;
			}

			l = snprintf(fpath, MAXPATHLEN, "%s/%s",
			    cf->file_path, dp->d_name);
			if (l == -1 || l >= MAXPATHLEN)
				fatal("cvs_file_walkdir: overflow");

			/*
			 * Anticipate the file type to sort them,
			 * note that we do not determine the final
			 * type until we actually have the fd floating
			 * around.
			 */
			if (dp->d_type == DT_DIR)
				cvs_file_get(fpath, &dl);
			else if (dp->d_type == DT_REG)
				cvs_file_get(fpath, &fl);

			cp += dp->d_reclen;
		}
	}

	if (nbytes == -1)
		fatal("cvs_file_walkdir: %s %s", cf->file_path,
		    strerror(errno));

	xfree(buf);

	while ((ip = TAILQ_FIRST(&dir_ign_pats)) != NULL) {
		TAILQ_REMOVE(&dir_ign_pats, ip, ip_list);
		xfree(ip);
	}

	entlist = cvs_ent_open(cf->file_path);
	TAILQ_FOREACH(line, &(entlist->cef_ent), entries_list) {
		ent = cvs_ent_parse(line->buf);

		l = snprintf(fpath, MAXPATHLEN, "%s/%s", cf->file_path,
		    ent->ce_name);
		if (l == -1 || l >= MAXPATHLEN)
			fatal("cvs_file_walkdir: overflow");

		if (!(cr->flags & CR_RECURSE_DIRS) &&
		    ent->ce_type == CVS_ENT_DIR)
			continue;

		if (ent->ce_type == CVS_ENT_DIR)
			cvs_file_get(fpath, &dl);
		else if (ent->ce_type == CVS_ENT_FILE)
			cvs_file_get(fpath, &fl);

		cvs_ent_free(ent);
	}

	cvs_ent_close(entlist, ENT_NOSYNC);

	if (cr->flags & CR_REPO) {
		repo = xmalloc(MAXPATHLEN);
		cvs_get_repository_path(cf->file_path, repo, MAXPATHLEN);
		cvs_repository_lock(repo);

		cvs_repository_getdir(repo, cf->file_path, &fl, &dl,
		    (cr->flags & CR_RECURSE_DIRS));
	}

	cvs_file_walklist(&fl, cr);
	cvs_file_freelist(&fl);

	if (cr->flags & CR_REPO) {
		cvs_repository_unlock(repo);
		xfree(repo);
	}

	cvs_file_walklist(&dl, cr);
	cvs_file_freelist(&dl);

	xfree(fpath);

	if (cr->leavedir != NULL)
		cr->leavedir(cf);
}

void
cvs_file_freelist(struct cvs_flisthead *fl)
{
	struct cvs_filelist *f;

	while ((f = TAILQ_FIRST(fl)) != NULL) {
		TAILQ_REMOVE(fl, f, flist);
		xfree(f->file_path);
		xfree(f);
	}
}

void
cvs_file_classify(struct cvs_file *cf, const char *tag, int loud)
{
	size_t len;
	time_t mtime;
	struct stat st;
	BUF *b1, *b2;
	int rflags, l, ismodified, rcsdead, verbose;
	CVSENTRIES *entlist = NULL;
	const char *state;
	char *repo, *rcsfile, r1[16], r2[16];

	cvs_log(LP_TRACE, "cvs_file_classify(%s)", cf->file_path);

	if (!strcmp(cf->file_path, ".")) {
		cf->file_status = FILE_UPTODATE;
		return;
	}

	verbose = (verbosity > 1 && loud == 1);

	repo = xmalloc(MAXPATHLEN);
	rcsfile = xmalloc(MAXPATHLEN);

	cvs_get_repository_path(cf->file_wd, repo, MAXPATHLEN);
	l = snprintf(rcsfile, MAXPATHLEN, "%s/%s",
	    repo, cf->file_name);
	if (l == -1 || l >= MAXPATHLEN)
		fatal("cvs_file_classify: overflow");

	if (cf->file_type == CVS_FILE) {
		len = strlcat(rcsfile, RCS_FILE_EXT, MAXPATHLEN);
		if (len >= MAXPATHLEN)
			fatal("cvs_file_classify: truncation");
	}

	cf->file_rpath = xstrdup(rcsfile);

	entlist = cvs_ent_open(cf->file_wd);
	cf->file_ent = cvs_ent_get(entlist, cf->file_name);

	if (cf->file_ent != NULL) {
		if (cf->file_ent->ce_type == CVS_ENT_DIR &&
		    cf->file_type != CVS_DIR)
			fatal("%s is supposed to be a directory, but it is not",
			    cf->file_path);
		if (cf->file_ent->ce_type == CVS_ENT_FILE &&
		    cf->file_type != CVS_FILE)
			fatal("%s is supposed to be a file, but it is not",
			    cf->file_path);
	}

	if (cf->file_type == CVS_DIR) {
		if (cf->fd == -1 && stat(rcsfile, &st) != -1)
			cf->file_status = DIR_CREATE;
		else if (cf->file_ent != NULL)
			cf->file_status = FILE_UPTODATE;
		else
			cf->file_status = FILE_UNKNOWN;

		xfree(repo);
		xfree(rcsfile);
		cvs_ent_close(entlist, ENT_NOSYNC);
		return;
	}

	rflags = RCS_READ;
	switch (cvs_cmdop) {
	case CVS_OP_COMMIT:
		rflags = RCS_WRITE;
		break;
	case CVS_OP_IMPORT:
	case CVS_OP_LOG:
		rflags |= RCS_PARSE_FULLY;
		break;
	}

	cf->repo_fd = open(cf->file_rpath, O_RDONLY);
	if (cf->repo_fd != -1) {
		cf->file_rcs = rcs_open(cf->file_rpath, cf->repo_fd, rflags);
		if (cf->file_rcs == NULL)
			fatal("cvs_file_classify: failed to parse RCS");
		cf->file_rcs->rf_inattic = 0;
	} else if (cvs_cmdop != CVS_OP_CHECKOUT) {
		l = snprintf(rcsfile, MAXPATHLEN, "%s/%s/%s%s",
		    repo, CVS_PATH_ATTIC, cf->file_name, RCS_FILE_EXT);
		if (l == -1 || l >= MAXPATHLEN)
			fatal("cvs_file_classify: overflow");

		cf->repo_fd = open(rcsfile, O_RDONLY);
		if (cf->repo_fd != -1) {
			xfree(cf->file_rpath);
			cf->file_rpath = xstrdup(rcsfile);
			cf->file_rcs = rcs_open(cf->file_rpath,
			     cf->repo_fd, rflags);
			if (cf->file_rcs == NULL)
				fatal("cvs_file_classify: failed to parse RCS");
			cf->file_rcs->rf_inattic = 1;
		} else {
			cf->file_rcs = NULL;
		}
	} else
		cf->file_rcs = NULL;

	if (tag != NULL && cf->file_rcs != NULL)
		cf->file_rcsrev = rcs_translate_tag(tag, cf->file_rcs);
	else if (cf->file_ent != NULL && cf->file_ent->ce_tag != NULL) {
		cf->file_rcsrev = rcsnum_alloc();
		rcsnum_cpy(cf->file_ent->ce_rev, cf->file_rcsrev, 0);
	} else if (cf->file_rcs != NULL)
		cf->file_rcsrev = rcs_head_get(cf->file_rcs);
	else
		cf->file_rcsrev = NULL;

	if (cf->file_ent != NULL)
		rcsnum_tostr(cf->file_ent->ce_rev, r1, sizeof(r1));
	if (cf->file_rcsrev != NULL)
		rcsnum_tostr(cf->file_rcsrev, r2, sizeof(r2));

	ismodified = rcsdead = 0;
	if (cf->fd != -1 && cf->file_ent != NULL) {
		if (fstat(cf->fd, &st) == -1)
			fatal("cvs_file_classify: %s", strerror(errno));

		mtime = cvs_hack_time(st.st_mtime, 1);
		if (mtime != cf->file_ent->ce_mtime)
			ismodified = 1;
	}

	if (ismodified == 1 && cf->fd != -1 && cf->file_rcs != NULL) {
		b1 = rcs_rev_getbuf(cf->file_rcs, cf->file_rcsrev);
		if (b1 == NULL)
			fatal("failed to get HEAD revision for comparison");

		b1 = rcs_kwexp_buf(b1, cf->file_rcs, cf->file_rcsrev);

		b2 = cvs_buf_load_fd(cf->fd, BUF_AUTOEXT);
		if (b2 == NULL)
			fatal("failed to get file content for comparison");

		/* b1 and b2 get released in cvs_buf_differ */
		if (cvs_buf_differ(b1, b2))
			ismodified = 1;
		else
			ismodified = 0;
	}

	if (cf->file_rcs != NULL) {
		state = rcs_state_get(cf->file_rcs, cf->file_rcsrev);
		if (state == NULL)
			fatal("failed to get state for HEAD for %s",
			    cf->file_path);
		if (!strcmp(state, RCS_STATE_DEAD))
			rcsdead = 1;

		cf->file_rcs->rf_dead = rcsdead;
	}

	/*
	 * 10 Sin
	 * 20 Goto hell
	 * (I welcome you if-else hell)
	 */
	if (cf->file_ent == NULL) {
		if (cf->file_rcs == NULL) {
			if (cf->fd == -1) {
				if (verbose)
					cvs_log(LP_NOTICE,
					    "nothing known about '%s'",
					    cf->file_path);
			} else {
				if (verbose)
					cvs_log(LP_NOTICE,
					    "use add to create an entry for %s",
					    cf->file_path);
			}

			cf->file_status = FILE_UNKNOWN;
		} else if (rcsdead == 1) {
			if (cf->fd == -1) {
				cf->file_status = FILE_UPTODATE;
			} else {
				if (verbose)
					cvs_log(LP_NOTICE,
					    "use add to create an entry for %s",
					    cf->file_path);
				cf->file_status = FILE_UNKNOWN;
			}
		} else {
			cf->file_status = FILE_CHECKOUT;
		}
	} else if (cf->file_ent->ce_status == CVS_ENT_ADDED) {
		if (cf->fd == -1) {
			if (verbose)
				cvs_log(LP_NOTICE,
				    "warning: new-born %s has disappeared",
				    cf->file_path);
			cf->file_status = FILE_REMOVE_ENTRY;
		} else if (cf->file_rcs == NULL || rcsdead == 1) {
			cf->file_status = FILE_ADDED;
		} else {
			if (verbose)
				cvs_log(LP_NOTICE,
				    "conflict: %s already created by others",
				    cf->file_path);
			cf->file_status = FILE_CONFLICT;
		}
	} else if (cf->file_ent->ce_status == CVS_ENT_REMOVED) {
		if (cf->fd != -1) {
			if (verbose)
				cvs_log(LP_NOTICE,
				    "%s should be removed but is still there",
				    cf->file_path);
			cf->file_status = FILE_REMOVED;
		} else if (cf->file_rcs == NULL || rcsdead == 1) {
			cf->file_status = FILE_REMOVE_ENTRY;
		} else {
			if (strcmp(r1, r2)) {
				if (verbose)
					cvs_log(LP_NOTICE,
					    "conflict: removed %s was modified"
					    " by a second party",
					    cf->file_path);
				cf->file_status = FILE_CONFLICT;
			} else {
				cf->file_status = FILE_REMOVED;
			}
		}
	} else if (cf->file_ent->ce_status == CVS_ENT_REG) {
		if (cf->file_rcs == NULL || rcsdead == 1) {
			if (cf->fd == -1) {
				if (verbose)
					cvs_log(LP_NOTICE,
					    "warning: %s's entry exists but"
					    " there is no longer a file"
					    " in the repository,"
					    " removing entry",
					     cf->file_path);
				cf->file_status = FILE_REMOVE_ENTRY;
			} else {
				if (ismodified) {
					if (verbose)
						cvs_log(LP_NOTICE,
						    "conflict: %s is no longer "
						    "in the repository but is "
						    "locally modified",
						    cf->file_path);
					cf->file_status = FILE_CONFLICT;
				} else {
					if (verbose)
						cvs_log(LP_NOTICE,
						    "%s is no longer in the "
						    "repository",
						    cf->file_path);

					cf->file_status = FILE_UNLINK;
				}
			}
		} else {
			if (cf->fd == -1) {
				if (verbose)
					cvs_log(LP_NOTICE,
					    "warning: %s was lost",
					    cf->file_path);
				cf->file_status = FILE_LOST;
			} else {
				if (ismodified == 1)
					cf->file_status = FILE_MODIFIED;
				else
					cf->file_status = FILE_UPTODATE;

				if (strcmp(r1, r2)) {
					if (cf->file_status == FILE_MODIFIED)
						cf->file_status = FILE_MERGE;
					else
						cf->file_status = FILE_PATCH;
				}
			}
		}
	}

	xfree(repo);
	xfree(rcsfile);
	cvs_ent_close(entlist, ENT_NOSYNC);
}

void
cvs_file_free(struct cvs_file *cf)
{
	xfree(cf->file_name);
	xfree(cf->file_wd);
	xfree(cf->file_path);

	if (cf->file_rcsrev != NULL)
		rcsnum_free(cf->file_rcsrev);
	if (cf->file_rpath != NULL)
		xfree(cf->file_rpath);
	if (cf->file_ent != NULL)
		cvs_ent_free(cf->file_ent);
	if (cf->file_rcs != NULL)
		rcs_close(cf->file_rcs);
	if (cf->fd != -1)
		(void)close(cf->fd);
	if (cf->repo_fd != -1)
		(void)close(cf->repo_fd);
	xfree(cf);
}

int
cvs_file_cmpname(const char *name1, const char *name2)
{
	return (cvs_nocase == 0) ? (strcmp(name1, name2)) :
	    (strcasecmp(name1, name2));
}

int
cvs_file_cmp(const char *file1, const char *file2)
{
	struct stat stb1, stb2;
	int fd1, fd2, ret;

	ret = 0;

	if ((fd1 = open(file1, O_RDONLY|O_NOFOLLOW, 0)) == -1)
		fatal("cvs_file_cmp: open: `%s': %s", file1, strerror(errno));
	if ((fd2 = open(file2, O_RDONLY|O_NOFOLLOW, 0)) == -1)
		fatal("cvs_file_cmp: open: `%s': %s", file2, strerror(errno));

	if (fstat(fd1, &stb1) == -1)
		fatal("cvs_file_cmp: `%s': %s", file1, strerror(errno));
	if (fstat(fd2, &stb2) == -1)
		fatal("cvs_file_cmp: `%s': %s", file2, strerror(errno));

	if (stb1.st_size != stb2.st_size ||
	    (stb1.st_mode & S_IFMT) != (stb2.st_mode & S_IFMT)) {
		ret = 1;
		goto out;
	}

	if (S_ISBLK(stb1.st_mode) || S_ISCHR(stb1.st_mode)) {
		if (stb1.st_rdev != stb2.st_rdev)
			ret = 1;
		goto out;
	}

	if (S_ISREG(stb1.st_mode)) {
		void *p1, *p2;

		if (stb1.st_size > SIZE_MAX) {
			ret = 1;
			goto out;
		}	

		if ((p1 = mmap(NULL, stb1.st_size, PROT_READ,
		    MAP_FILE, fd1, (off_t)0)) == MAP_FAILED)
			fatal("cvs_file_cmp: mmap failed");

		if ((p2 = mmap(NULL, stb1.st_size, PROT_READ,
		    MAP_FILE, fd2, (off_t)0)) == MAP_FAILED)
			fatal("cvs_file_cmp: mmap failed");

		madvise(p1, stb1.st_size, MADV_SEQUENTIAL);
		madvise(p2, stb1.st_size, MADV_SEQUENTIAL);

		ret = memcmp(p1, p2, stb1.st_size);

		(void)munmap(p1, stb1.st_size);
		(void)munmap(p2, stb1.st_size);
	}

out:
	(void)close(fd1);
	(void)close(fd2);

	return (ret);
}

int
cvs_file_copy(const char *from, const char *to)
{
	struct stat st;
	struct timeval tv[2];
	time_t atime, mtime;
	int src, dst, ret;

	ret = 0;

	cvs_log(LP_TRACE, "cvs_file_copy(%s,%s)", from, to);

	if (cvs_noexec == 1)
		return (0);

	if ((src = open(from, O_RDONLY, 0)) == -1)
		fatal("cvs_file_copy: open: `%s': %s", from, strerror(errno));

	if (fstat(src, &st) == -1)
		fatal("cvs_file_copy: `%s': %s", from, strerror(errno));

	atime = st.st_atimespec.tv_sec;
	mtime = st.st_mtimespec.tv_sec;

	if (S_ISREG(st.st_mode)) {
		size_t sz;
		ssize_t nw;
		char *p, *buf;
		int saved_errno;

		if (st.st_size > SIZE_MAX) {
			ret = -1;
			goto out;
		}	

		if ((dst = open(to, O_CREAT|O_TRUNC|O_WRONLY,
		    st.st_mode & (S_IRWXU|S_IRWXG|S_IRWXO))) == -1)
			fatal("cvs_file_copy: open `%s': %s",
			    to, strerror(errno));

		if ((p = mmap(NULL, st.st_size, PROT_READ,
		    MAP_FILE, src, (off_t)0)) == MAP_FAILED) {
			saved_errno = errno;
			(void)unlink(to);
			fatal("cvs_file_copy: mmap: %s", strerror(saved_errno));
		}

		madvise(p, st.st_size, MADV_SEQUENTIAL);

		sz = st.st_size;
		buf = p;

		while (sz > 0) {
			if ((nw = write(dst, p, sz)) == -1) {
				saved_errno = errno;
				(void)unlink(to);
				fatal("cvs_file_copy: `%s': %s",
				    from, strerror(saved_errno));
			}
			buf += nw;
			sz -= nw;
		}

		(void)munmap(p, st.st_size);

		tv[0].tv_sec = atime;
		tv[1].tv_sec = mtime;

		if (futimes(dst, tv) == -1) {
			saved_errno = errno;
			(void)unlink(to);
			fatal("cvs_file_copy: futimes: %s",
			    strerror(saved_errno));
		}
		(void)close(dst);
	}
out:
	(void)close(src);

	return (ret);
}
