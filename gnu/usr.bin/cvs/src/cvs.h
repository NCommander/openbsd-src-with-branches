/* $CVSid: @(#)cvs.h 1.86 94/10/22 $	 */

/*
 * basic information used in all source files
 *
 */


#include "config.h"		/* this is stuff found via autoconf */
#include "options.h"		/* these are some larger questions which
				   can't easily be automatically checked
				   for */

/* AIX requires this to be the first thing in the file. */
#ifdef __GNUC__
#define alloca __builtin_alloca
#else /* not __GNUC__ */
#if HAVE_ALLOCA_H
#include <alloca.h>
#else /* not HAVE_ALLOCA_H */
#ifdef _AIX
 #pragma alloca
#else /* not _AIX */
char *alloca ();
#endif /* not _AIX */
#endif /* not HAVE_ALLOCA_H */
#endif /* not __GNUC__ */

/* Changed from if __STDC__ to ifdef __STDC__ because of Sun's acc compiler */

#ifdef __STDC__
#define	PTR	void *
#else
#define	PTR	char *
#endif

/* Add prototype support.  */
#ifndef PROTO
#if defined (USE_PROTOTYPES) ? USE_PROTOTYPES : defined (__STDC__)
#define PROTO(ARGS) ARGS
#else
#define PROTO(ARGS) ()
#endif
#endif

#if __GNUC__ == 2
#define USE(var) static const char sizeof##var = sizeof(sizeof##var) + sizeof(var)
#else
#define USE(var) static const char standalone_semis_illegal_sigh
#endif


#include <stdio.h>

#ifdef STDC_HEADERS
#include <stdlib.h>
#else
extern void exit ();
extern char *getenv();
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#ifdef SERVER_SUPPORT
/* If the system doesn't provide strerror, it won't be declared in
   string.h.  */
char *strerror ();
#endif

#include <fnmatch.h>		/* This is supposed to be available on Posix systems */

#include <ctype.h>
#include <pwd.h>
#include <signal.h>

#ifdef HAVE_ERRNO_H
#include <errno.h>
#else
#ifndef errno
extern int errno;
#endif /* !errno */
#endif /* HAVE_ERRNO_H */

#include "system.h"

#include "hash.h"
#if defined(SERVER_SUPPORT) || defined(CLIENT_SUPPORT)
#include "server.h"
#include "client.h"
#endif

#ifdef MY_NDBM
#include "myndbm.h"
#else
#include <ndbm.h>
#endif /* MY_NDBM */

#include "regex.h"
#include "getopt.h"
#include "wait.h"

/* Define to enable alternate death support (which uses the RCS state).  */
#define DEATH_STATE 1

#define DEATH_SUPPORT 1

#include "rcs.h"


/* XXX - for now this is static */
#ifndef PATH_MAX
#ifdef MAXPATHLEN
#define	PATH_MAX MAXPATHLEN+2
#else
#define	PATH_MAX 1024+2
#endif
#endif /* PATH_MAX */

/* just in case this implementation does not define this */
#ifndef L_tmpnam
#define	L_tmpnam	50
#endif


/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 *
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 *
 * Definitions for the CVS Administrative directory and the files it contains.
 * Here as #define's to make changing the names a simple task.
 */
#define	CVSADM		"CVS"
#define	CVSADM_ENT	"CVS/Entries"
#define	CVSADM_ENTBAK	"CVS/Entries.Backup"
#define CVSADM_ENTLOG	"CVS/Entries.Log"
#define	CVSADM_ENTSTAT	"CVS/Entries.Static"
#define	CVSADM_REP	"CVS/Repository"
#define	CVSADM_ROOT	"CVS/Root"
#define	CVSADM_CIPROG	"CVS/Checkin.prog"
#define	CVSADM_UPROG	"CVS/Update.prog"
#define	CVSADM_TAG	"CVS/Tag"

/*
 * Definitions for the CVSROOT Administrative directory and the files it
 * contains.  This directory is created as a sub-directory of the $CVSROOT
 * environment variable, and holds global administration information for the
 * entire source repository beginning at $CVSROOT.
 */
#define	CVSROOTADM		"CVSROOT"
#define	CVSROOTADM_MODULES	"modules"
#define	CVSROOTADM_LOGINFO	"loginfo"
#define	CVSROOTADM_RCSINFO	"rcsinfo"
#define CVSROOTADM_COMMITINFO	"commitinfo"
#define CVSROOTADM_TAGINFO      "taginfo"
#define	CVSROOTADM_EDITINFO	"editinfo"
#define	CVSROOTADM_HISTORY	"history"
#define	CVSROOTADM_IGNORE	"cvsignore"
#define	CVSROOTADM_CHECKOUTLIST "checkoutlist"
#define CVSROOTADM_WRAPPER      "cvswrappers"
#define CVSNULLREPOS		"Emptydir"	/* an empty directory */

/* support for the modules file (CVSROOTADM_MODULES) */
#define	CVSMODULE_OPTS	"ad:i:lo:e:s:t:u:"/* options in modules file */
#define CVSMODULE_SPEC	'&'		/* special delimiter */

/* Other CVS file names */

/* Files go in the attic if the head main branch revision is dead,
   otherwise they go in the regular repository directories.  The whole
   concept of having an attic is sort of a relic from before death
   support but on the other hand, it probably does help the speed of
   some operations (such as main branch checkouts and updates).  */
#define	CVSATTIC	"Attic"

#define	CVSLCK		"#cvs.lock"
#define	CVSTFL		"#cvs.tfl"
#define	CVSRFL		"#cvs.rfl"
#define	CVSWFL		"#cvs.wfl"
#define CVSRFLPAT	"#cvs.rfl.*"	/* wildcard expr to match read locks */
#define	CVSEXT_OPT	",p"
#define	CVSEXT_LOG	",t"
#define	CVSPREFIX	",,"
#define CVSDOTIGNORE	".cvsignore"
#define CVSDOTWRAPPER   ".cvswrappers"

/* miscellaneous CVS defines */
#define	CVSEDITPREFIX	"CVS: "
#define	CVSLCKAGE	(60*60)		/* 1-hour old lock files cleaned up */
#define	CVSLCKSLEEP	30		/* wait 30 seconds before retrying */
#define	CVSBRANCH	"1.1.1"		/* RCS branch used for vendor srcs */
#define	BAKPREFIX	".#"		/* when rcsmerge'ing */
#ifndef DEVNULL
#define	DEVNULL		"/dev/null"
#endif

#define	FALSE		0
#define	TRUE		1

/*
 * Special tags. -rHEAD	refers to the head of an RCS file, regardless of any
 * sticky tags. -rBASE	refers to the current revision the user has checked
 * out This mimics the behaviour of RCS.
 */
#define	TAG_HEAD	"HEAD"
#define	TAG_BASE	"BASE"

/* Environment variable used by CVS */
#define	CVSREAD_ENV	"CVSREAD"	/* make files read-only */
#define	CVSREAD_DFLT	FALSE		/* writable files by default */

#define	CVSREADONLYFS_ENV "CVSREADONLYFS" /* repository is read-only */

#define	RCSBIN_ENV	"RCSBIN"	/* RCS binary directory */
/* #define	RCSBIN_DFLT		   Set by config.h */

#define	EDITOR1_ENV	"CVSEDITOR"	/* which editor to use */
#define	EDITOR2_ENV	"VISUAL"	/* which editor to use */
#define	EDITOR3_ENV	"EDITOR"	/* which editor to use */
/* #define	EDITOR_DFLT		   Set by config.h */

#define	CVSROOT_ENV	"CVSROOT"	/* source directory root */
#define	CVSROOT_DFLT	NULL		/* No dflt; must set for checkout */

#define	IGNORE_ENV	"CVSIGNORE"	/* More files to ignore */
#define WRAPPER_ENV     "CVSWRAPPERS"   /* name of the wrapper file */

/*
 * If the beginning of the Repository matches the following string, strip it
 * so that the output to the logfile does not contain a full pathname.
 *
 * If the CVSROOT environment variable is set, it overrides this define.
 */
#define	REPOS_STRIP	"/master/"

/*
 * The maximum number of files per each CVS directory. This is mainly for
 * sizing arrays statically rather than dynamically.  3000 seems plenty for
 * now.
 */
#define	MAXFILEPERDIR	3000
#define	MAXLINELEN	5000		/* max input line from a file */
#define	MAXPROGLEN	30000		/* max program length to system() */
#define	MAXLISTLEN	40000		/* For [A-Z]list holders */
#define MAXDATELEN	50		/* max length for a date */

/* structure of a entry record */
struct entnode
{
    char *version;
    char *timestamp;
    char *options;
    char *tag;
    char *date;
    char *conflict;
};
typedef struct entnode Entnode;

/* The type of request that is being done in do_module() */
enum mtype
{
    CHECKOUT, TAG, PATCH, EXPORT
};

/*
 * defines for Classify_File() to determine the current state of a file.
 * These are also used as types in the data field for the list we make for
 * Update_Logfile in commit, import, and add.
 */
enum classify_type
{
    T_UNKNOWN = 1,			/* no old-style analog existed	 */
    T_CONFLICT,				/* C (conflict) list		 */
    T_NEEDS_MERGE,			/* G (needs merging) list	 */
    T_MODIFIED,				/* M (needs checked in) list 	 */
    T_CHECKOUT,				/* O (needs checkout) list	 */
    T_ADDED,				/* A (added file) list		 */
    T_REMOVED,				/* R (removed file) list	 */
    T_REMOVE_ENTRY,			/* W (removed entry) list	 */
    T_UPTODATE,				/* File is up-to-date		 */
#ifdef SERVER_SUPPORT
    T_PATCH,				/* P Like C, but can patch	 */
#endif
    T_TITLE				/* title for node type 		 */
};
typedef enum classify_type Ctype;

/*
 * a struct vers_ts contains all the information about a file including the
 * user and rcs file names, and the version checked out and the head.
 *
 * this is usually obtained from a call to Version_TS which takes a tag argument
 * for the RCS file if desired
 */
struct vers_ts
{
    char *vn_user;			/* rcs version user file derives from
					 * it can have the following special
					 * values:
					 *    empty = no user file
					 *    0 = user file is new
					 *    -vers = user file to be removed */
    char *vn_rcs;			/* the version for the rcs file
					 * (tag version?) 	 */
    char *ts_user;			/* the timestamp for the user file */
    char *ts_rcs;			/* the user timestamp from entries */
    char *options;			/* opts from Entries file
					 * (keyword expansion)	 */
    char *ts_conflict;			/* Holds time_stamp of conflict */
    char *tag;				/* tag stored in the Entries file */
    char *date;				/* date stored in the Entries file */
    Entnode *entdata;			/* pointer to entries file node  */
    RCSNode *srcfile;			/* pointer to parsed src file info */
};
typedef struct vers_ts Vers_TS;

/*
 * structure used for list-private storage by Entries_Open() and
 * Version_TS().
 */
struct stickydirtag
{
    int aflag;
    char *tag;
    char *date;
    char *options;
};

/* Flags for find_{names,dirs} routines */
#define W_LOCAL			0x01	/* look for files locally */
#define W_REPOS			0x02	/* look for files in the repository */
#define W_ATTIC			0x04	/* look for files in the attic */

/* Flags for return values of direnter procs for the recursion processor */
enum direnter_type
{
    R_PROCESS = 1,			/* process files and maybe dirs */
    R_SKIP_FILES,			/* don't process files in this dir */
    R_SKIP_DIRS,			/* don't process sub-dirs */
    R_SKIP_ALL				/* don't process files or dirs */
};
typedef enum direnter_type Dtype;

extern char *program_name, *command_name;
extern char *Rcsbin, *Editor, *CVSroot;
#ifdef CVSADM_ROOT
extern char *CVSADM_Root;
extern int cvsadmin_root;
#endif /* CVSADM_ROOT */
extern char *CurDir;
extern int really_quiet, quiet;
extern int use_editor;
extern int cvswrite;

extern int trace;		/* Show all commands */
extern int noexec;		/* Don't modify disk anywhere */
extern int readonlyfs;		/* fail on all write locks; succeed all read locks */
extern int logoff;		/* Don't write history entry */

extern char hostname[];

/* Externs that are included directly in the CVS sources */
int RCS_settag PROTO((const char *, const char *, const char *));
int RCS_deltag PROTO((const char *, const char *, int));
int RCS_setbranch PROTO((const char *, const char *));
int RCS_lock PROTO((const char *, const char *, int));
int RCS_unlock PROTO((const char *, const char *, int));
int RCS_merge PROTO((const char *, const char *, const char *, const char *));

#include "error.h"

DBM *open_module PROTO((void));
FILE *open_file PROTO((const char *, const char *));
List *Find_Dirs PROTO((char *repository, int which));
void Entries_Close PROTO((List *entries));
List *Entries_Open PROTO((int aflag));
char *Make_Date PROTO((char *rawdate));
char *Name_Repository PROTO((char *dir, char *update_dir));
#ifdef CVSADM_ROOT
char *Name_Root PROTO((char *dir, char *update_dir));
void Create_Root PROTO((char *dir, char *rootdir));
int same_directories PROTO((char *dir1, char *dir2));
#endif /* CVSADM_ROOT */
char *Short_Repository PROTO((char *repository));
char *gca PROTO((char *rev1, char *rev2));
char *getcaller PROTO((void));
char *time_stamp PROTO((char *file));
char *xmalloc PROTO((size_t bytes));
char *xrealloc PROTO((char *ptr, size_t bytes));
char *xstrdup PROTO((const char *str));
int No_Difference PROTO((char *file, Vers_TS * vers, List * entries,
			 char *repository, char *update_dir));
int Parse_Info PROTO((char *infofile, char *repository, int PROTO((*callproc)) PROTO(()), int all));
int Reader_Lock PROTO((char *xrepository));
int SIG_register PROTO((int sig, RETSIGTYPE PROTO((*fn)) PROTO(())));
int Writer_Lock PROTO((List * list));
int ign_name PROTO((char *name));
int isdir PROTO((const char *file));
int isfile PROTO((const char *file));
int islink PROTO((const char *file));
int isreadable PROTO((const char *file));
int iswritable PROTO((const char *file));
int isabsolute PROTO((const char *filename));
char *last_component PROTO((char *path));

int joining PROTO((void));
int numdots PROTO((const char *s));
int unlink_file PROTO((const char *f));
int unlink_file_dir PROTO((const char *f));
int update PROTO((int argc, char *argv[]));
int xcmp PROTO((const char *file1, const char *file2));
int yesno PROTO((void));
time_t get_date PROTO((char *date, struct timeb *now));
void Create_Admin PROTO((char *dir, char *update_dir,
			 char *repository, char *tag, char *date));
void Lock_Cleanup PROTO((void));
void ParseTag PROTO((char **tagp, char **datep));
void Scratch_Entry PROTO((List * list, char *fname));
void WriteTag PROTO((char *dir, char *tag, char *date));
void cat_module PROTO((int status));
void check_entries PROTO((char *dir));
void close_module PROTO((DBM * db));
void copy_file PROTO((const char *from, const char *to));
void (*error_set_cleanup PROTO((void (*) (void)))) PROTO ((void));
void fperror PROTO((FILE * fp, int status, int errnum, char *message,...));
void free_names PROTO((int *pargc, char *argv[]));
void freevers_ts PROTO((Vers_TS ** versp));
void ign_add PROTO((char *ign, int hold));
void ign_add_file PROTO((char *file, int hold));
void ign_setup PROTO((void));
void ign_dir_add PROTO((char *name));
int ignore_directory PROTO((char *name));
void line2argv PROTO((int *pargc, char *argv[], char *line));
void make_directories PROTO((const char *name));
void make_directory PROTO((const char *name));
void rename_file PROTO((const char *from, const char *to));
void strip_path PROTO((char *path));
void strip_trailing_slashes PROTO((char *path));
void update_delproc PROTO((Node * p));
void usage PROTO((const char *const *cpp));
void xchmod PROTO((char *fname, int writable));
char *xgetwd PROTO((void));
int Checkin PROTO((int type, char *file, char *update_dir,
		   char *repository, char *rcs, char *rev,
		   char *tag, char *options, char *message, List *entries));
Ctype Classify_File PROTO((char *file, char *tag, char *date, char *options,
		     int force_tag_match, int aflag, char *repository,
		     List *entries, List *srcfiles, Vers_TS **versp,
		     char *update_dir, int pipeout));
List *Find_Names PROTO((char *repository, int which, int aflag,
		  List ** optentries));
void Register PROTO((List * list, char *fname, char *vn, char *ts,
	       char *options, char *tag, char *date, char *ts_conflict));
void Update_Logfile PROTO((char *repository, char *xmessage, char *xrevision,
		     FILE * xlogfp, List * xchanges));
Vers_TS *Version_TS PROTO((char *repository, char *options, char *tag,
		     char *date, char *user, int force_tag_match,
		     int set_time, List * entries, List * xfiles));
void do_editor PROTO((char *dir, char **messagep,
		      char *repository, List * changes));
int do_module PROTO((DBM * db, char *mname, enum mtype m_type, char *msg,
	       int PROTO((*callback_proc)) (), char *where, int shorten,
	       int local_specified, int run_module_prog, char *extra_arg));
int do_recursion PROTO((int PROTO((*xfileproc)) (), int PROTO((*xfilesdoneproc)) (),
		  Dtype PROTO((*xdirentproc)) (), int PROTO((*xdirleaveproc)) (),
		  Dtype xflags, int xwhich, int xaflag, int xreadlock,
		  int xdosrcs));
int do_update PROTO((int argc, char *argv[], char *xoptions, char *xtag,
	       char *xdate, int xforce, int local, int xbuild,
	       int xaflag, int xprune, int xpipeout, int which,
	       char *xjoin_rev1, char *xjoin_rev2, char *preload_update_dir));
void history_write PROTO((int type, char *update_dir, char *revs, char *name,
		    char *repository));
int start_recursion PROTO((int PROTO((*fileproc)) (), int PROTO((*filesdoneproc)) (),
		     Dtype PROTO((*direntproc)) (), int PROTO((*dirleaveproc)) (),
		     int argc, char *argv[], int local, int which,
		     int aflag, int readlock, char *update_preload,
		     int dosrcs, int wd_is_repos));
void SIG_beginCrSect PROTO((void));
void SIG_endCrSect PROTO((void));
void read_cvsrc PROTO((int *argc, char ***argv));

char *make_message_rcslegal PROTO((char *message));

/* flags for run_exec(), the fast system() for CVS */
#define	RUN_NORMAL		0x0000	/* no special behaviour */
#define	RUN_COMBINED		0x0001	/* stdout is duped to stderr */
#define	RUN_REALLY		0x0002	/* do the exec, even if noexec is on */
#define	RUN_STDOUT_APPEND	0x0004	/* append to stdout, don't truncate */
#define	RUN_STDERR_APPEND	0x0008	/* append to stderr, don't truncate */
#define	RUN_SIGIGNORE		0x0010	/* ignore interrupts for command */
#define	RUN_TTY		(char *)0	/* for the benefit of lint */

void run_arg PROTO((const char *s));
void run_print PROTO((FILE * fp));
#ifdef HAVE_VPRINTF
void run_setup PROTO((const char *fmt,...));
void run_args PROTO((const char *fmt,...));
#else
void run_setup ();
void run_args ();
#endif
int run_exec PROTO((char *stin, char *stout, char *sterr, int flags));

/* other similar-minded stuff from run.c.  */
FILE *Popen PROTO((const char *, const char *));
int piped_child PROTO((char **, int *, int *));
void close_on_exec PROTO((int));
int filter_stream_through_program PROTO((int, int, char **, pid_t *));

pid_t waitpid PROTO((pid_t, int *, int));

/* Wrappers.  */

typedef enum { WRAP_MERGE, WRAP_COPY } WrapMergeMethod;
typedef enum { WRAP_TOCVS, WRAP_FROMCVS, WRAP_CONFLICT } WrapMergeHas;

void  wrap_setup PROTO((void));
int   wrap_name_has PROTO((const char *name,WrapMergeHas has));
char *wrap_tocvs_process_file PROTO((const char *fileName));
int   wrap_merge_is_copy PROTO((const char *fileName));
char *wrap_fromcvs_process_file PROTO((const char *fileName));
void wrap_add_file PROTO((const char *file,int temp));
void wrap_add PROTO((char *line,int temp));
