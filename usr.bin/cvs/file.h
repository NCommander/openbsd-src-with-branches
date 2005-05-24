/*	$OpenBSD: file.h,v 1.19 2005/05/20 05:13:44 joris Exp $	*/
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

#ifndef FILE_H
#define FILE_H

#include <sys/param.h>

#include <dirent.h>
#include <search.h>

#include "rcs.h"

struct cvs_file;
struct cvs_entries;


#define CVS_FILE_MAXDEPTH     32


#define CF_STAT     0x01  /* obsolete */
#define CF_IGNORE   0x02  /* apply regular ignore rules */
#define CF_RECURSE  0x04  /* recurse on directory operations */
#define CF_SORT     0x08  /* all files are sorted alphabetically */
#define CF_KNOWN    0x10  /* only recurse in directories known to CVS */
#define CF_CREATE   0x20  /* create if file does not exist */
#define CF_MKADMIN  0x40  /* create administrative files if they're missing */
#define CF_NOSYMS   0x80  /* ignore symbolic links */
#define CF_NOFILES  0x100 /* don't load any files inside a directory */

/*
 * The cvs_file structure is used to represent any file or directory within
 * the CVS tree's hierarchy.  The <cf_path> field is a path relative to the
 * directory in which the cvs command was executed.  The <cf_parent> field
 * points back to the parent node in the directory tree structure (it is
 * NULL if the directory is at the wd of the command).
 *
 * The <cf_cvstat> field gives the file's status with regards to the CVS
 * repository.  The file can be in any one of the CVS_FST_* states.
 */
#define CVS_FST_UNKNOWN   0
#define CVS_FST_UPTODATE  1
#define CVS_FST_MODIFIED  2
#define CVS_FST_ADDED     3
#define CVS_FST_REMOVED   4
#define CVS_FST_CONFLICT  5
#define CVS_FST_PATCHED   6
#define CVS_FST_LOST      7


SIMPLEQ_HEAD(cvs_flist, cvs_file);

typedef struct cvs_file {
	struct cvs_file  *cf_parent;  /* parent directory (NULL if none) */
	const char       *cf_name;
	mode_t            cf_mode;
	u_int8_t          cf_cvstat;  /* cvs status of the file */
	u_int8_t          cf_type;    /* uses values from dirent.h */
	u_int16_t         cf_flags;

	union {
		struct {
			RCSNUM  *cd_lrev;	/* local revision */
			time_t   cd_mtime;
			char    *cd_tag;
			char    *cd_opts;
		} cf_reg;
		struct {
			char             *cd_repo;
			struct cvsroot   *cd_root;
			struct cvs_flist  cd_files;
		} cf_dir;
	} cf_td;

	SIMPLEQ_ENTRY(cvs_file)  cf_list;
} CVSFILE;

/* only valid for regular files */
#define cf_mtime  cf_td.cf_reg.cd_mtime
#define cf_lrev   cf_td.cf_reg.cd_lrev
#define cf_tag    cf_td.cf_reg.cd_tag
#define cf_opts   cf_td.cf_reg.cd_opts

/* only valid for directories */
#define cf_files  cf_td.cf_dir.cd_files
#define cf_repo   cf_td.cf_dir.cd_repo
#define cf_root   cf_td.cf_dir.cd_root

#define CVS_FILE_NAME(cf)   (cf->cf_name)



#define CVS_DIRF_STATIC    0x01
#define CVS_DIRF_STICKY    0x02
#define CVS_DIRF_BASE      0x04

#define CVS_GDIR_IGNORE    0x08

#define CVS_DIR_ROOT(f)  ((((f)->cf_type == DT_DIR) && \
	((f)->cf_root != NULL)) ? (f)->cf_root : \
	(((f)->cf_parent == NULL) ? NULL : (f)->cf_parent->cf_root))

#define CVS_DIR_REPO(f)  (((f)->cf_type == DT_DIR) ? \
	(f)->cf_repo : (((f)->cf_parent == NULL) ? \
	NULL : (f)->cf_parent->cf_repo))

int      cvs_file_init    (void);
int      cvs_file_ignore  (const char *);
int      cvs_file_chkign  (const char *);
CVSFILE* cvs_file_get     (const char *, int, int (*)(CVSFILE *, void *), void *);
CVSFILE* cvs_file_getspec (char **, int, int, int (*)(CVSFILE *, void *), void *);
CVSFILE* cvs_file_create  (CVSFILE *, const char *, u_int, mode_t);
CVSFILE* cvs_file_copy    (CVSFILE *);
int      cvs_file_attach  (CVSFILE *, CVSFILE *);
int      cvs_file_examine (CVSFILE *, int (*)(CVSFILE *, void *), void *);

int      cvs_file_init    (void);
int      cvs_file_ignore  (const char *);
int      cvs_file_chkign  (const char *);
CVSFILE* cvs_file_load    (const char *, int);
CVSFILE* cvs_file_find    (CVSFILE *, const char *);
char*    cvs_file_getpath (CVSFILE *, char *, size_t);
void     cvs_file_free    (CVSFILE *);


#endif /* FILE_H */
