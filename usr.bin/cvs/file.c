/*	$OpenBSD: file.c,v 1.65 2005/04/18 22:28:41 joris Exp $	*/
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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <pwd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <libgen.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fnmatch.h>

#include "cvs.h"
#include "log.h"
#include "file.h"
#include "strtab.h"


#define CVS_IGN_STATIC    0x01     /* pattern is static, no need to glob */

#define CVS_CHAR_ISMETA(c)  ((c == '*') || (c == '?') || (c == '['))


/* ignore pattern */
struct cvs_ignpat {
	char  ip_pat[MAXNAMLEN];
	int   ip_flags;
	TAILQ_ENTRY (cvs_ignpat) ip_list;
};


/*
 * Standard patterns to ignore.
 */
static const char *cvs_ign_std[] = {
	".",
	"..",
	"*.o",
	"*.so",
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
	"*.core",
	".#*",
	"*~",
	"_$*",
	"*$",
#ifdef OLD_SMELLY_CRUFT
	"RCSLOG",
	"tags",
	"TAGS",
	"RCS",
	"SCCS",
	"cvslog.*",	/* to ignore CVS_CLIENT_LOG output */
	"#*",
	",*",
#endif
};


/*
 * Entries in the CVS/Entries file with a revision of '0' have only been
 * added.  Compare against this revision to see if this is the case
 */
static RCSNUM *cvs_addedrev;


TAILQ_HEAD(, cvs_ignpat)  cvs_ign_pats;


static int      cvs_file_getdir  (CVSFILE *, int);
static int      cvs_file_sort    (struct cvs_flist *, u_int);
static int      cvs_file_cmp     (const void *, const void *);
static int      cvs_file_cmpname (const char *, const char *);
static CVSFILE* cvs_file_alloc   (const char *, u_int);
static CVSFILE* cvs_file_lget  (const char *, int, CVSFILE *, struct cvs_ent *);



/*
 * cvs_file_init()
 *
 */
int
cvs_file_init(void)
{
	int i, l;
	size_t len;
	char path[MAXPATHLEN], buf[MAXNAMLEN];
	FILE *ifp;
	struct passwd *pwd;

	TAILQ_INIT(&cvs_ign_pats);

	if ((cvs_addedrev = rcsnum_parse("0")) == NULL)
		return (-1);

	/* standard patterns to ignore */
	for (i = 0; i < (int)(sizeof(cvs_ign_std)/sizeof(char *)); i++)
		cvs_file_ignore(cvs_ign_std[i]);

	/* read the cvsignore file in the user's home directory, if any */
	pwd = getpwuid(getuid());
	if (pwd != NULL) {
		l = snprintf(path, sizeof(path), "%s/.cvsignore", pwd->pw_dir);
		if (l == -1 || l >= (int)sizeof(path)) {
			errno = ENAMETOOLONG;
			cvs_log(LP_ERRNO, "%s", path);
			return (-1);
		}

		ifp = fopen(path, "r");
		if (ifp == NULL) {
			if (errno != ENOENT)
				cvs_log(LP_ERRNO,
				    "failed to open user's cvsignore", path);
		} else {
			while (fgets(buf, sizeof(buf), ifp) != NULL) {
				len = strlen(buf);
				if (len == 0)
					continue;
				if (buf[len - 1] != '\n') {
					cvs_log(LP_ERR, "line too long in `%s'",
					    path);
				}
				buf[--len] = '\0';
				cvs_file_ignore(buf);
			}
			(void)fclose(ifp);
		}
	}

	return (0);
}


/*
 * cvs_file_ignore()
 *
 * Add the pattern <pat> to the list of patterns for files to ignore.
 * Returns 0 on success, or -1 on failure.
 */
int
cvs_file_ignore(const char *pat)
{
	char *cp;
	struct cvs_ignpat *ip;

	ip = (struct cvs_ignpat *)malloc(sizeof(*ip));
	if (ip == NULL) {
		cvs_log(LP_ERR, "failed to allocate space for ignore pattern");
		return (-1);
	}

	strlcpy(ip->ip_pat, pat, sizeof(ip->ip_pat));

	/* check if we will need globbing for that pattern */
	ip->ip_flags = CVS_IGN_STATIC;
	for (cp = ip->ip_pat; *cp != '\0'; cp++) {
		if (CVS_CHAR_ISMETA(*cp)) {
			ip->ip_flags &= ~CVS_IGN_STATIC;
			break;
		}
	}

	TAILQ_INSERT_TAIL(&cvs_ign_pats, ip, ip_list);

	return (0);
}


/*
 * cvs_file_chkign()
 *
 * Returns 1 if the filename <file> is matched by one of the ignore
 * patterns, or 0 otherwise.
 */
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

	return (0);
}


/*
 * cvs_file_create()
 *
 * Create a new file whose path is specified in <path> and of type <type>.
 * If the type is DT_DIR, the CVS administrative repository and files will be
 * created.
 * Returns the created file on success, or NULL on failure.
 */
CVSFILE*
cvs_file_create(CVSFILE *parent, const char *path, u_int type, mode_t mode)
{
	int fd;
	char fp[MAXPATHLEN];
	CVSFILE *cfp;
	CVSENTRIES *ent;

	cfp = cvs_file_alloc(path, type);
	if (cfp == NULL)
		return (NULL);

	cfp->cf_mode = mode;
	cfp->cf_parent = parent;

	if (type == DT_DIR) {
		cfp->cf_root = cvsroot_get(path);
		cfp->cf_repo = strdup(cvs_file_getpath(cfp,
		    fp, sizeof(fp)));
		if (cfp->cf_repo == NULL) {
			cvs_file_free(cfp);
			return (NULL);
		}

		if ((mkdir(path, mode) == -1) || (cvs_mkadmin(cfp, mode) < 0)) {
			cvs_file_free(cfp);
			return (NULL);
		}

		ent = cvs_ent_open(path, O_RDWR);
		if (ent != NULL) {
			cvs_ent_close(ent);
		}
	} else {
		fd = open(path, O_WRONLY|O_CREAT|O_EXCL, mode);
		if (fd == -1) {
			cvs_file_free(cfp);
			return (NULL);
		}
		(void)close(fd);
	}

	return (cfp);
}


/*
 * cvs_file_copy()
 *
 * Allocate space to create a copy of the file <orig>.  The copy inherits all
 * of the original's attributes, but does not inherit its children if the
 * original file is a directory.  Note that files copied using this mechanism
 * are linked to their parent, but the parent has no link to the file.  This
 * is so cvs_file_getpath() works.
 * Returns the copied file on success, or NULL on failure.  The returned
 * structure should be freed using cvs_file_free().
 */
CVSFILE*
cvs_file_copy(CVSFILE *orig)
{
	char path[MAXPATHLEN];
	CVSFILE *cfp;

	cvs_file_getpath(orig, path, sizeof(path));

	cfp = cvs_file_alloc(path, orig->cf_type);
	if (cfp == NULL)
		return (NULL);

	cfp->cf_parent = orig->cf_parent;
	cfp->cf_mode = orig->cf_mode;
	cfp->cf_cvstat = orig->cf_cvstat;

	if (orig->cf_type == DT_REG)
		cfp->cf_mtime = orig->cf_mtime;
	else if (orig->cf_type == DT_DIR) {
		/* XXX copy CVS directory attributes */
	}

	return (cfp);
}


/*
 * cvs_file_get()
 *
 * Load a cvs_file structure with all the information pertaining to the file
 * <path>.
 * The <flags> parameter specifies various flags that alter the behaviour of
 * the function.  The CF_RECURSE flag causes the function to recursively load
 * subdirectories when <path> is a directory.
 * The CF_SORT flag causes the files to be sorted in alphabetical order upon
 * loading.  The special case of "." as a path specification generates
 * recursion for a single level and is equivalent to calling cvs_file_get() on
 * all files of that directory.
 * Returns a pointer to the cvs file structure, which must later be freed
 * with cvs_file_free().
 */

CVSFILE*
cvs_file_get(const char *path, int flags)
{
	return cvs_file_lget(path, flags, NULL, NULL);
}


/*
 * cvs_file_getspec()
 *
 * Load a specific set of files whose paths are given in the vector <fspec>,
 * whose size is given in <fsn>.
 * Returns a pointer to the lowest common subdirectory to all specified
 * files.
 */
CVSFILE*
cvs_file_getspec(char **fspec, int fsn, int flags)
{
	int i;
	char *sp, *np, pcopy[MAXPATHLEN];
	CVSFILE *base, *cf, *nf;

	base = cvs_file_get(".", 0);
	if (base == NULL)
		return (NULL);

	for (i = 0; i < fsn; i++) {
		strlcpy(pcopy, fspec[i], sizeof(pcopy));
		cf = base;
		sp = pcopy;

		do {
			np = strchr(sp, '/');
			if (np != NULL)
				*np = '\0';
			nf = cvs_file_find(cf, sp);
			if (nf == NULL) {
				nf = cvs_file_lget(pcopy, 0, cf, NULL);
				if (nf == NULL) {
					cvs_file_free(base);
					return (NULL);
				}

				if (cvs_file_attach(cf, nf) < 0) {
					cvs_file_free(base);
					return (NULL);
				}
			}

			if (np != NULL) {
				*np = '/';
				sp = np + 1;
			}

			cf = nf;
		} while (np != NULL);
	}

	return (base);
}


/*
 * cvs_file_find()
 *
 * Find the pointer to a CVS file entry within the file hierarchy <hier>.
 * The file's pathname <path> must be relative to the base of <hier>.
 * Returns the entry on success, or NULL on failure.
 */
CVSFILE*
cvs_file_find(CVSFILE *hier, const char *path)
{
	char *pp, *sp, pbuf[MAXPATHLEN];
	CVSFILE *sf, *cf;

	strlcpy(pbuf, path, sizeof(pbuf));

	cf = hier;
	pp = pbuf;
	do {
		sp = strchr(pp, '/');
		if (sp != NULL)
			*(sp++) = '\0';

		/* special case */
		if (*pp == '.') {
			if ((*(pp + 1) == '.') && (*(pp + 2) == '\0')) {
				/* request to go back to parent */
				if (cf->cf_parent == NULL) {
					cvs_log(LP_NOTICE,
					    "path %s goes back too far", path);
					return (NULL);
				}
				cf = cf->cf_parent;
				continue;
			} else if (*(pp + 1) == '\0')
				continue;
		}

		SIMPLEQ_FOREACH(sf, &(cf->cf_files), cf_list)
			if (cvs_file_cmpname(pp, CVS_FILE_NAME(sf)) == 0)
				break;
		if (sf == NULL)
			return (NULL);

		cf = sf;
		pp = sp;
	} while (sp != NULL);

	return (cf);
}


/*
 * cvs_file_getpath()
 *
 * Get the full path of the file <file> and store it in <buf>, which is of
 * size <len>.  For portability, it is recommended that <buf> always be
 * at least MAXPATHLEN bytes long.
 * Returns a pointer to the start of the path on success, or NULL on failure.
 */
char*
cvs_file_getpath(CVSFILE *file, char *buf, size_t len)
{
	u_int i;
	char *fp, *namevec[CVS_FILE_MAXDEPTH];
	CVSFILE *top;

	buf[0] = '\0';
	i = CVS_FILE_MAXDEPTH;
	memset(namevec, 0, sizeof(namevec));

	/* find the top node */
	for (top = file; (top != NULL) && (i > 0); top = top->cf_parent) {
		fp = CVS_FILE_NAME(top);

		/* skip self-references */
		if ((fp[0] == '.') && (fp[1] == '\0'))
			continue;
		namevec[--i] = fp;
	}

	if (i == 0)
		return (NULL);
	else if (i == CVS_FILE_MAXDEPTH) {
		strlcpy(buf, ".", len);
		return (buf);
	}

	while (i < CVS_FILE_MAXDEPTH - 1) {
		strlcat(buf, namevec[i++], len);
		strlcat(buf, "/", len);
	}
	strlcat(buf, namevec[i], len);

	return (buf);
}


/*
 * cvs_file_attach()
 *
 * Attach the file <file> as one of the children of parent <parent>, which
 * has to be a file of type DT_DIR.
 * Returns 0 on success, or -1 on failure.
 */
int
cvs_file_attach(CVSFILE *parent, CVSFILE *file)
{
	if (parent->cf_type != DT_DIR)
		return (-1);

	SIMPLEQ_INSERT_TAIL(&(parent->cf_files), file, cf_list);
	file->cf_parent = parent;

	return (0);
}


/*
 * cvs_file_getdir()
 *
 * Get a cvs directory structure for the directory whose path is <dir>.
 * This function should not free the directory information on error, as this
 * is performed by cvs_file_free().
 */
static int
cvs_file_getdir(CVSFILE *cf, int flags)
{
	int ret, fd, l;
	u_int ndirs, nfiles;
	long base;
	u_char *dp, *ep;
	char fbuf[2048], pbuf[MAXPATHLEN], fpath[MAXPATHLEN];
	struct dirent *ent;
	CVSFILE *cfp;
	CVSENTRIES *entfile;
	struct stat st;
	struct cvs_ent *cvsent;
	struct cvs_flist dirs;

	ndirs = 0;
	nfiles = 0;
	SIMPLEQ_INIT(&dirs);

	cvs_file_getpath(cf, fpath, sizeof(fpath));

	cf->cf_root = cvsroot_get(fpath);
	if (cf->cf_root == NULL)
		return (-1);

	if (cf->cf_cvstat != CVS_FST_UNKNOWN) {
		if (flags & CF_MKADMIN)
			cvs_mkadmin(cf, 0755);

		/* if the CVS administrative directory exists, load the info */
		l = snprintf(pbuf, sizeof(pbuf), "%s/" CVS_PATH_CVSDIR, fpath);
		if (l == -1 || l >= (int)sizeof(pbuf)) {
			errno = ENAMETOOLONG;
			cvs_log(LP_ERRNO, "%s", pbuf);
			return (-1);
		}

		if ((stat(pbuf, &st) == 0) && S_ISDIR(st.st_mode)) {
			if (cvs_readrepo(fpath, pbuf, sizeof(pbuf)) == 0) {
				cf->cf_repo = strdup(pbuf);
				if (cf->cf_repo == NULL) {
					cvs_log(LP_ERRNO,
					    "failed to dup repository string");
					return (-1);
				}
			}

			entfile = cvs_ent_open(fpath, O_RDONLY);
		}
	}

	if ((flags & CF_KNOWN) && (cf->cf_cvstat == CVS_FST_UNKNOWN))
		return (0);

	fd = open(fpath, O_RDONLY);
	if (fd == -1) {
		cvs_log(LP_ERRNO, "failed to open `%s'", fpath);
		return (-1);
	}

	/* To load all files, we first get the entries for the directory and
	 * load the information for each of those entries.  The handle to
	 * the Entries file kept in the directory data is only temporary and
	 * the files should remove their entry when they use it.  After all
	 * files in the directory have been processed, the Entries handle
	 * should only be left with those entries for which no real file
	 * exists.  We then build file structures for those files too, as
	 * we will likely receive fresh copies from the server as part of the
	 * response.
	 */
	do {
		ret = getdirentries(fd, fbuf, sizeof(fbuf), &base);
		if (ret == -1) {
			cvs_log(LP_ERRNO, "failed to get directory entries");
			(void)close(fd);
			return (-1);
		}

		dp = fbuf;
		ep = fbuf + (size_t)ret;
		while (dp < ep) {
			ent = (struct dirent *)dp;
			dp += ent->d_reclen;
			if (ent->d_fileno == 0)
				continue;

			if ((flags & CF_IGNORE) && cvs_file_chkign(ent->d_name))
				continue;

			if ((flags & CF_NOSYMS) && (ent->d_type == DT_LNK))
				continue;

			if (!(flags & CF_RECURSE) && (ent->d_type == DT_DIR)) {
				if (entfile != NULL)
					(void)cvs_ent_remove(entfile,
					    ent->d_name);
				continue;
			}

			l = snprintf(pbuf, sizeof(pbuf), "%s/%s", fpath,
			    ent->d_name);
			if (l == -1 || l >= (int)sizeof(pbuf)) {
				errno = ENAMETOOLONG;
				cvs_log(LP_ERRNO, "%s", pbuf);
				
				(void)close(fd);
				return (-1);
			}

			if (entfile != NULL)
				cvsent = cvs_ent_get(entfile, ent->d_name);
			cfp = cvs_file_lget(pbuf, flags, cf, cvsent);
			if (cfp == NULL) {
				(void)close(fd);
				return (-1);
			}
			if (entfile != NULL)
				cvs_ent_remove(entfile, cfp->cf_name);

			if (cfp->cf_type == DT_DIR) {
				SIMPLEQ_INSERT_TAIL(&dirs, cfp, cf_list);
				ndirs++;
			} else {
				SIMPLEQ_INSERT_TAIL(&(cf->cf_files), cfp,
				    cf_list);
				nfiles++;
			}
		}
	} while (ret > 0);

	if (entfile != NULL) {
		/* now create file structure for files which have an
		 * entry in the Entries file but no file on disk
		 */
		while ((cvsent = cvs_ent_next(entfile)) != NULL) {
			l = snprintf(pbuf, sizeof(pbuf), "%s/%s", fpath,
			    cvsent->ce_name);
			if (l == -1 || l >= (int)sizeof(pbuf)) {
				errno = ENAMETOOLONG;
				cvs_log(LP_ERRNO, "%s", pbuf);

				(void)close(fd);
				return (-1);
			}

			cfp = cvs_file_lget(pbuf, flags, cf, cvsent);
			if (cfp != NULL) {
				if (cfp->cf_type == DT_DIR) {
					SIMPLEQ_INSERT_TAIL(&dirs, cfp, cf_list);
					ndirs++;
				} else {
					SIMPLEQ_INSERT_TAIL(&(cf->cf_files), cfp,
					    cf_list);
					nfiles++;
				}
			}
		}
		cvs_ent_close(entfile);
	}

	if (flags & CF_SORT) {
		cvs_file_sort(&(cf->cf_files), nfiles);
		cvs_file_sort(&dirs, ndirs);
	}

	while (!SIMPLEQ_EMPTY(&dirs)) {
		cfp = SIMPLEQ_FIRST(&dirs);
		SIMPLEQ_REMOVE_HEAD(&dirs, cf_list);
		SIMPLEQ_INSERT_TAIL(&(cf->cf_files), cfp, cf_list);
	}

	(void)close(fd);
	return (0);
}


/*
 * cvs_file_free()
 *
 * Free a cvs_file structure and its contents.
 */
void
cvs_file_free(CVSFILE *cf)
{
	CVSFILE *child;

	if (cf->cf_name != NULL)
		cvs_strfree(cf->cf_name);

	if (cf->cf_type == DT_DIR) {
		if (cf->cf_root != NULL)
			cvsroot_free(cf->cf_root);
		if (cf->cf_repo != NULL)
			free(cf->cf_repo);
		while (!SIMPLEQ_EMPTY(&(cf->cf_files))) {
			child = SIMPLEQ_FIRST(&(cf->cf_files));
			SIMPLEQ_REMOVE_HEAD(&(cf->cf_files), cf_list);
			cvs_file_free(child);
		}
	} else {
		if (cf->cf_tag != NULL)
			cvs_strfree(cf->cf_tag);
	}

	free(cf);
}


/*
 * cvs_file_examine()
 *
 * Examine the contents of the CVS file structure <cf> with the function
 * <exam>.  The function is called for all subdirectories and files of the
 * root file.
 */
int
cvs_file_examine(CVSFILE *cf, int (*exam)(CVSFILE *, void *), void *arg)
{
	int ret;
	CVSFILE *fp;

	if (cf->cf_type == DT_DIR) {
		ret = (*exam)(cf, arg);
		SIMPLEQ_FOREACH(fp, &(cf->cf_files), cf_list) {
			ret = cvs_file_examine(fp, exam, arg);
			if (ret != 0)
				break;
		}
	} else
		ret = (*exam)(cf, arg);

	return (ret);
}

/*
 * cvs_file_sort()
 *
 * Sort a list of cvs file structures according to their filename.  The list
 * <flp> is modified according to the sorting algorithm.  The number of files
 * in the list must be given by <nfiles>.
 * Returns 0 on success, or -1 on failure.
 */
static int
cvs_file_sort(struct cvs_flist *flp, u_int nfiles)
{
	int i;
	size_t nb;
	CVSFILE *cf, **cfvec;

	cfvec = (CVSFILE **)calloc(nfiles, sizeof(CVSFILE *));
	if (cfvec == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate sorting vector");
		return (-1);
	}

	i = 0;
	SIMPLEQ_FOREACH(cf, flp, cf_list) {
		if (i == (int)nfiles) {
			cvs_log(LP_WARN, "too many files to sort");
			/* rebuild the list and abort sorting */
			while (--i >= 0)
				SIMPLEQ_INSERT_HEAD(flp, cfvec[i], cf_list);
			free(cfvec);
			return (-1);
		}
		cfvec[i++] = cf;

		/* now unlink it from the list,
		 * we'll put it back in order later
		 */
		SIMPLEQ_REMOVE_HEAD(flp, cf_list);
	}

	/* clear the list just in case */
	SIMPLEQ_INIT(flp);
	nb = (size_t)i;

	heapsort(cfvec, nb, sizeof(cf), cvs_file_cmp);

	/* rebuild the list from the bottom up */
	for (i = (int)nb - 1; i >= 0; i--)
		SIMPLEQ_INSERT_HEAD(flp, cfvec[i], cf_list);

	free(cfvec);
	return (0);
}


static int
cvs_file_cmp(const void *f1, const void *f2)
{
	const CVSFILE *cf1, *cf2;
	cf1 = *(CVSFILE * const *)f1;
	cf2 = *(CVSFILE * const *)f2;
	return cvs_file_cmpname(CVS_FILE_NAME(cf1), CVS_FILE_NAME(cf2));
}


/*
 * cvs_file_alloc()
 *
 * Allocate a CVSFILE structure and initialize its internals.
 */
CVSFILE*
cvs_file_alloc(const char *path, u_int type)
{
	CVSFILE *cfp;

	cfp = (CVSFILE *)malloc(sizeof(*cfp));
	if (cfp == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate CVS file data");
		return (NULL);
	}
	memset(cfp, 0, sizeof(*cfp));

	cfp->cf_type = type;
	cfp->cf_cvstat = CVS_FST_UNKNOWN;

	if (type == DT_DIR) {
		SIMPLEQ_INIT(&(cfp->cf_files));
	}

	cfp->cf_name = cvs_strdup(basename(path));
	if (cfp->cf_name == NULL) {
		cvs_log(LP_ERR, "failed to copy file name");
		cvs_file_free(cfp);
		return (NULL);
	}

	return (cfp);
}


/*
 * cvs_file_lget()
 *
 * Get the file and link it with the parent right away.
 * Returns a pointer to the created file structure on success, or NULL on
 * failure.
 */
static CVSFILE*
cvs_file_lget(const char *path, int flags, CVSFILE *parent, struct cvs_ent *ent)
{
	int ret, cwd;
	u_int type;
	struct stat st;
	CVSFILE *cfp;

	type = DT_UNKNOWN;
	cwd = (strcmp(path, ".") == 0) ? 1 : 0;

	ret = stat(path, &st);
	if (ret == 0)
		type = IFTODT(st.st_mode);

	if ((cfp = cvs_file_alloc(path, type)) == NULL)
		return (NULL);
	cfp->cf_parent = parent;

	if ((cfp->cf_type == DT_DIR) && (cfp->cf_parent == NULL))
		cfp->cf_flags |= CVS_DIRF_BASE;

	if (ret == 0) {
		cfp->cf_mode = st.st_mode & ACCESSPERMS;
		if (cfp->cf_type == DT_REG)
			cfp->cf_mtime = st.st_mtime;

		if (ent == NULL)
			cfp->cf_cvstat = (cwd == 1) ?
			    CVS_FST_UPTODATE : CVS_FST_UNKNOWN;
		else {
			/* always show directories as up-to-date */
			if (ent->ce_type == CVS_ENT_DIR)
				cfp->cf_cvstat = CVS_FST_UPTODATE;
			else if (rcsnum_cmp(ent->ce_rev, cvs_addedrev, 2) == 0)
				cfp->cf_cvstat = CVS_FST_ADDED;
			else {
				/* check last modified time */
				if (ent->ce_mtime >= (time_t)st.st_mtime)
					cfp->cf_cvstat = CVS_FST_UPTODATE;
				else
					cfp->cf_cvstat = CVS_FST_MODIFIED;
			}
		}
	} else {
		if (ent == NULL) {
			cvs_log(LP_ERR, "no Entry and no file for `%s'",
			    CVS_FILE_NAME(cfp));
			cvs_file_free(cfp);
			return (NULL);
		} else {
			if (ent->ce_type == CVS_ENT_FILE)
				cfp->cf_type = DT_REG;
			else if (ent->ce_type == CVS_ENT_DIR)
				cfp->cf_type = DT_DIR;
			else
				cvs_log(LP_WARN, "unknown ce_type %d", 
				    ent->ce_type);
			cfp->cf_cvstat = CVS_FST_LOST;
		}
	}

	if (ent != NULL) {
		/* steal the RCSNUM */
		cfp->cf_lrev = ent->ce_rev;
		if (ent->ce_tag != NULL) {
			if ((cfp->cf_tag = cvs_strdup(ent->ce_tag)) == NULL) {
				cvs_file_free(cfp);
				return (NULL);
			}
		}
		ent->ce_rev = NULL;
	}

	if ((cfp->cf_type == DT_DIR) && (cvs_file_getdir(cfp, flags) < 0)) {
		cvs_file_free(cfp);
		return (NULL);
	}

	return (cfp);
}


static int
cvs_file_cmpname(const char *name1, const char *name2)
{
	return (cvs_nocase == 0) ? (strcmp(name1, name2)) :
	    (strcasecmp(name1, name2));
}
