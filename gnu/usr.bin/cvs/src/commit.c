/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * Commit Files
 * 
 * "commit" commits the present version to the RCS repository, AFTER
 * having done a test on conflicts.
 *
 * The call is: cvs commit [options] files...
 * 
 */

#include "cvs.h"
#include "getline.h"
#include "edit.h"
#include "fileattr.h"

static Dtype check_direntproc PROTO((char *dir, char *repos, char *update_dir));
static int check_fileproc PROTO((struct file_info *finfo));
static int check_filesdoneproc PROTO((int err, char *repos, char *update_dir));
static int checkaddfile PROTO((char *file, char *repository, char *tag,
			       char *options, RCSNode **rcsnode)); 
static Dtype commit_direntproc PROTO((char *dir, char *repos, char *update_dir));
static int commit_dirleaveproc PROTO((char *dir, int err, char *update_dir));
static int commit_fileproc PROTO((struct file_info *finfo));
static int commit_filesdoneproc PROTO((int err, char *repository, char *update_dir));
static int finaladd PROTO((char *file, char *revision, char *tag,
			   char *options, char *update_dir,
			   char *repository, List *entries));
static int findmaxrev PROTO((Node * p, void *closure));
static int lock_RCS PROTO((char *user, char *rcs, char *rev, char *repository));
static int lockrcsfile PROTO((char *file, char *repository, char *rev));
static int precommit_list_proc PROTO((Node * p, void *closure));
static int precommit_proc PROTO((char *repository, char *filter));
static int remove_file PROTO((char *file, char *repository, char *tag,
			char *message, List *entries, RCSNode *rcsnode));
static void fix_rcs_modes PROTO((char *rcs, char *user));
static void fixaddfile PROTO((char *file, char *repository));
static void fixbranch PROTO((char *file, char *repository, char *branch));
static void unlockrcs PROTO((char *file, char *repository));
static void ci_delproc PROTO((Node *p));
static void masterlist_delproc PROTO((Node *p));
static void locate_rcs PROTO((char *file, char *repository, char *rcs));

struct commit_info
{
    Ctype status;			/* as returned from Classify_File() */
    char *rev;				/* a numeric rev, if we know it */
    char *tag;				/* any sticky tag, or -r option */
    char *options;			/* Any sticky -k option */
};
struct master_lists
{
    List *ulist;			/* list for Update_Logfile */
    List *cilist;			/* list with commit_info structs */
};

static int force_ci = 0;
static int got_message;
static int run_module_prog = 1;
static int aflag;
static char *tag;
static char *write_dirtag;
static char *logfile;
static List *mulist;
static char *message;
static time_t last_register_time;


static const char *const commit_usage[] =
{
    "Usage: %s %s [-nRlf] [-m msg | -F logfile] [-r rev] files...\n",
    "\t-n\tDo not run the module program (if any).\n",
    "\t-R\tProcess directories recursively.\n",
    "\t-l\tLocal directory only (not recursive).\n",
    "\t-f\tForce the file to be committed; disables recursion.\n",
    "\t-F file\tRead the log message from file.\n",
    "\t-m msg\tLog message.\n",
    "\t-r rev\tCommit to this branch or trunk revision.\n",
    NULL
};

#ifdef CLIENT_SUPPORT
struct find_data {
    List *ulist;
    int argc;
    char **argv;
};

/* Pass as a static until we get around to fixing start_recursion to
   pass along a void * where we can stash it.  */
struct find_data *find_data_static;

static int find_fileproc PROTO ((struct file_info *finfo));

/* Machinery to find out what is modified, added, and removed.  It is
   possible this should be broken out into a new client_classify function;
   merging it with classify_file is almost sure to be a mess, though,
   because classify_file has all kinds of repository processing.  */
static int
find_fileproc (finfo)
    struct file_info *finfo;
{
    Vers_TS *vers;
    enum classify_type status;
    Node *node;
    struct find_data *args = find_data_static;

    vers = Version_TS ((char *)NULL, (char *)NULL, (char *)NULL,
		       (char *)NULL,
		       finfo->file, 0, 0, finfo->entries, (RCSNode *)NULL);
    if (vers->ts_user == NULL
	&& vers->vn_user != NULL
	&& vers->vn_user[0] == '-')
	status = T_REMOVED;
    else if (vers->vn_user == NULL)
    {
	if (vers->ts_user == NULL)
	    error (0, 0, "nothing known about `%s'", finfo->fullname);
	else
	    error (0, 0, "use `cvs add' to create an entry for %s",
		   finfo->fullname);
	return 1;
    }
    else if (vers->ts_user != NULL
	     && vers->vn_user != NULL
	     && vers->vn_user[0] == '0')
	status = T_ADDED;
    else if (vers->ts_user != NULL
	     && vers->ts_rcs != NULL
	     && strcmp (vers->ts_user, vers->ts_rcs) != 0)
	status = T_MODIFIED;
    else
    {
	/* This covers unmodified files, as well as a variety of other
	   cases.  FIXME: we probably should be printing a message and
	   returning 1 for many of those cases (but I'm not sure
	   exactly which ones).  */
	return 0;
    }

    node = getnode ();
    node->key = xstrdup (finfo->fullname);

    node->type = UPDATE;
    node->delproc = update_delproc;
    node->data = (char *) status;
    (void)addnode (args->ulist, node);

    ++args->argc;

    return 0;
}

static int copy_ulist PROTO ((Node *, void *));

static int
copy_ulist (node, data)
    Node *node;
    void *data;
{
    struct find_data *args = (struct find_data *)data;
    args->argv[args->argc++] = node->key;
    return 0;
}
#endif /* CLIENT_SUPPORT */

int
commit (argc, argv)
    int argc;
    char **argv;
{
    int c;
    int err = 0;
    int local = 0;

    if (argc == -1)
	usage (commit_usage);

#ifdef CVS_BADROOT
    /*
     * For log purposes, do not allow "root" to commit files.  If you look
     * like root, but are really logged in as a non-root user, it's OK.
     */
    if (geteuid () == (uid_t) 0)
    {
	struct passwd *pw;

	if ((pw = (struct passwd *) getpwnam (getcaller ())) == NULL)
	    error (1, 0, "you are unknown to this system");
	if (pw->pw_uid == (uid_t) 0)
	    error (1, 0, "cannot commit files as 'root'");
    }
#endif /* CVS_BADROOT */

    optind = 1;
    while ((c = getopt (argc, argv, "nlRm:fF:r:")) != -1)
    {
	switch (c)
	{
	    case 'n':
		run_module_prog = 0;
		break;
	    case 'm':
#ifdef FORCE_USE_EDITOR
		use_editor = TRUE;
#else
		use_editor = FALSE;
#endif
		if (message)
		{
		    free (message);
		    message = NULL;
		}

		message = xstrdup(optarg);
		break;
	    case 'r':
		if (tag)
		    free (tag);
		tag = xstrdup (optarg);
		break;
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
	    case 'f':
		force_ci = 1;
		local = 1;		/* also disable recursion */
		break;
	    case 'F':
#ifdef FORCE_USE_EDITOR
		use_editor = TRUE;
#else
		use_editor = FALSE;
#endif
		logfile = optarg;
		break;
	    case '?':
	    default:
		usage (commit_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    /* numeric specified revision means we ignore sticky tags... */
    if (tag && isdigit (*tag))
    {
	aflag = 1;
	/* strip trailing dots */
	while (tag[strlen (tag) - 1] == '.')
	    tag[strlen (tag) - 1] = '\0';
    }

    /* some checks related to the "-F logfile" option */
    if (logfile)
    {
	int n, logfd;
	struct stat statbuf;

	if (message)
	    error (1, 0, "cannot specify both a message and a log file");

	/* FIXME: Why is this binary?  Needs more investigation.  */
	if ((logfd = open (logfile, O_RDONLY | OPEN_BINARY)) < 0)
	    error (1, errno, "cannot open log file %s", logfile);

	if (fstat(logfd, &statbuf) < 0)
	    error (1, errno, "cannot find size of log file %s", logfile);

	message = xmalloc (statbuf.st_size + 1);

	/* FIXME: Should keep reading until EOF, rather than assuming the
	   first read gets the whole thing.  */
	if ((n = read (logfd, message, statbuf.st_size + 1)) < 0)
	    error (1, errno, "cannot read log message from %s", logfile);

	(void) close (logfd);
	message[n] = '\0';
    }

#ifdef CLIENT_SUPPORT
    if (client_active) 
    {
	struct find_data find_args;

	ign_setup ();

	/* Note that we don't do ignore file processing here, and we
	   don't call ignore_files.  This means that we won't print "?
	   foo" for stray files.  Sounds OK, the doc only promises
	   that update does that.  */
	find_args.ulist = getlist ();
	find_args.argc = 0;
	find_data_static = &find_args;
	err = start_recursion (find_fileproc, (FILESDONEPROC) NULL,
				(DIRENTPROC) NULL, (DIRLEAVEPROC) NULL,
				argc, argv, local, W_LOCAL, 0, 0,
				(char *)NULL, 0, 0);
	if (err)
	    error (1, 0, "correct above errors first!");

	if (find_args.argc == 0)
	    return 0;

	/* Now we keep track of which files we actually are going to
	   operate on, and only work with those files in the future.
	   This saves time--we don't want to search the file system
	   of the working directory twice.  */
	find_args.argv = (char **) xmalloc (find_args.argc * sizeof (char **));
	find_args.argc = 0;
	walklist (find_args.ulist, copy_ulist, &find_args);

	/*
	 * Do this before calling do_editor; don't ask for a log
	 * message if we can't talk to the server.  But do it after we
	 * have made the checks that we can locally (to more quickly
	 * catch syntax errors, the case where no files are modified,
	 * added or removed, etc.).  */
	start_server ();

	/*
	 * We do this once, not once for each directory as in normal CVS.
	 * The protocol is designed this way.  This is a feature.
	 */
	if (use_editor)
	    do_editor (".", &message, (char *)NULL, find_args.ulist);

	/* We always send some sort of message, even if empty.  */
	option_with_arg ("-m", message);

	if (local)
	    send_arg("-l");
	if (force_ci)
	    send_arg("-f");
	if (!run_module_prog)
	    send_arg("-n");
	option_with_arg ("-r", tag);

	/* Sending only the names of the files which were modified, added,
	   or removed means that the server will only do an up-to-date
	   check on those files.  This is different from local CVS and
	   previous versions of client/server CVS, but it probably is a Good
	   Thing, or at least Not Such A Bad Thing.  */
	send_file_names (find_args.argc, find_args.argv, 0);
	send_files (find_args.argc, find_args.argv, local, 0);

	send_to_server ("ci\012", 0);
	return get_responses_and_close ();
    }
#endif

    if (tag != NULL)
	tag_check_valid (tag, argc, argv, local, aflag, "");

    /* XXX - this is not the perfect check for this */
    if (argc <= 0)
	write_dirtag = tag;

    wrap_setup ();

    lock_tree_for_write (argc, argv, local, aflag);

    /*
     * Set up the master update list
     */
    mulist = getlist ();

    /*
     * Run the recursion processor to verify the files are all up-to-date
     */
    err = start_recursion (check_fileproc, check_filesdoneproc,
			   check_direntproc, (DIRLEAVEPROC) NULL, argc,
			   argv, local, W_LOCAL, aflag, 0, (char *) NULL, 1,
			   0);
    if (err)
    {
	lock_tree_cleanup ();
	error (1, 0, "correct above errors first!");
    }

    /*
     * Run the recursion processor to commit the files
     */
    if (noexec == 0)
	err = start_recursion (commit_fileproc, commit_filesdoneproc,
			       commit_direntproc, commit_dirleaveproc,
			       argc, argv, local, W_LOCAL, aflag, 0,
			       (char *) NULL, 1, 0);

    /*
     * Unlock all the dirs and clean up
     */
    lock_tree_cleanup ();
    dellist (&mulist);

    if (last_register_time)
    {
	time_t now;

	(void) time (&now);
	if (now == last_register_time) 
	{
	    sleep (1);			/* to avoid time-stamp races */
	}
    }

    return (err);
}

/*
 * Check to see if a file is ok to commit and make sure all files are
 * up-to-date
 */
/* ARGSUSED */
static int
check_fileproc (finfo)
    struct file_info *finfo;
{
    Ctype status;
    char *xdir;
    Node *p;
    List *ulist, *cilist;
    Vers_TS *vers;
    struct commit_info *ci;
    int save_noexec, save_quiet, save_really_quiet;

    save_noexec = noexec;
    save_quiet = quiet;
    save_really_quiet = really_quiet;
    noexec = quiet = really_quiet = 1;

    /* handle specified numeric revision specially */
    if (tag && isdigit (*tag))
    {
	/* If the tag is for the trunk, make sure we're at the head */
	if (numdots (tag) < 2)
	{
	    status = Classify_File (finfo->file, (char *) NULL, (char *) NULL,
				    (char *) NULL, 1, aflag, finfo->repository,
				    finfo->entries, finfo->rcs, &vers, finfo->update_dir, 0);
	    if (status == T_UPTODATE || status == T_MODIFIED ||
		status == T_ADDED)
	    {
		Ctype xstatus;

		freevers_ts (&vers);
		xstatus = Classify_File (finfo->file, tag, (char *) NULL,
					 (char *) NULL, 1, aflag, finfo->repository,
					 finfo->entries, finfo->rcs, &vers, finfo->update_dir,
					 0);
		if (xstatus == T_REMOVE_ENTRY)
		    status = T_MODIFIED;
		else if (status == T_MODIFIED && xstatus == T_CONFLICT)
		    status = T_MODIFIED;
		else
		    status = xstatus;
	    }
	}
	else
	{
	    char *xtag, *cp;

	    /*
	     * The revision is off the main trunk; make sure we're
	     * up-to-date with the head of the specified branch.
	     */
	    xtag = xstrdup (tag);
	    if ((numdots (xtag) & 1) != 0)
	    {
		cp = strrchr (xtag, '.');
		*cp = '\0';
	    }
	    status = Classify_File (finfo->file, xtag, (char *) NULL,
				    (char *) NULL, 1, aflag, finfo->repository,
				    finfo->entries, finfo->rcs, &vers, finfo->update_dir, 0);
	    if ((status == T_REMOVE_ENTRY || status == T_CONFLICT)
		&& (cp = strrchr (xtag, '.')) != NULL)
	    {
		/* pluck one more dot off the revision */
		*cp = '\0';
		freevers_ts (&vers);
		status = Classify_File (finfo->file, xtag, (char *) NULL,
					(char *) NULL, 1, aflag, finfo->repository,
					finfo->entries, finfo->rcs, &vers, finfo->update_dir,
					0);
		if (status == T_UPTODATE || status == T_REMOVE_ENTRY)
		    status = T_MODIFIED;
	    }
	    /* now, muck with vers to make the tag correct */
	    free (vers->tag);
	    vers->tag = xstrdup (tag);
	    free (xtag);
	}
    }
    else
	status = Classify_File (finfo->file, tag, (char *) NULL, (char *) NULL,
				1, 0, finfo->repository, finfo->entries, finfo->rcs, &vers,
				finfo->update_dir, 0);
    noexec = save_noexec;
    quiet = save_quiet;
    really_quiet = save_really_quiet;

    /*
     * If the force-commit option is enabled, and the file in question
     * appears to be up-to-date, just make it look modified so that
     * it will be committed.
     */
    if (force_ci && status == T_UPTODATE)
	status = T_MODIFIED;

    switch (status)
    {
	case T_CHECKOUT:
#ifdef SERVER_SUPPORT
	case T_PATCH:
#endif
	case T_NEEDS_MERGE:
	case T_CONFLICT:
	case T_REMOVE_ENTRY:
	    error (0, 0, "Up-to-date check failed for `%s'", finfo->fullname);
	    freevers_ts (&vers);
	    return (1);
	case T_MODIFIED:
	case T_ADDED:
	case T_REMOVED:
	    /*
	     * some quick sanity checks; if no numeric -r option specified:
	     *	- can't have a sticky date
	     *	- can't have a sticky tag that is not a branch
	     * Also,
	     *	- if status is T_REMOVED, can't have a numeric tag
	     *	- if status is T_ADDED, rcs file must not exist
	     *	- if status is T_ADDED, can't have a non-trunk numeric rev
	     *	- if status is T_MODIFIED and a Conflict marker exists, don't
	     *    allow the commit if timestamp is identical or if we find
	     *    an RCS_MERGE_PAT in the file.
	     */
	    if (!tag || !isdigit (*tag))
	    {
		if (vers->date)
		{
		    error (0, 0,
			   "cannot commit with sticky date for file `%s'",
			   finfo->fullname);
		    freevers_ts (&vers);
		    return (1);
		}
		if (status == T_MODIFIED && vers->tag &&
		    !RCS_isbranch (finfo->rcs, vers->tag))
		{
		    error (0, 0,
			   "sticky tag `%s' for file `%s' is not a branch",
			   vers->tag, finfo->fullname);
		    freevers_ts (&vers);
		    return (1);
		}
	    }
	    if (status == T_MODIFIED && !force_ci && vers->ts_conflict)
	    {
		char *filestamp;
		int retcode;

		/*
		 * We found a "conflict" marker.
		 *
		 * If the timestamp on the file is the same as the
		 * timestamp stored in the Entries file, we block the commit.
		 */
#ifdef SERVER_SUPPORT
		if (server_active)
		    retcode = vers->ts_conflict[0] != '=';
		else {
		    filestamp = time_stamp (finfo->file);
		    retcode = strcmp (vers->ts_conflict, filestamp);
		    free (filestamp);
		}
#else
		filestamp = time_stamp (finfo->file);
		retcode = strcmp (vers->ts_conflict, filestamp);
		free (filestamp);
#endif
		if (retcode == 0)
		{
		    error (0, 0,
			  "file `%s' had a conflict and has not been modified",
			   finfo->fullname);
		    freevers_ts (&vers);
		    return (1);
		}

		/*
		 * If the timestamps differ, look for Conflict indicators
		 * in the file to see if we should block the commit anyway
		 */
		run_setup ("%s", GREP);
		run_arg (RCS_MERGE_PAT);
		run_arg (finfo->file);
		retcode = run_exec (RUN_TTY, DEVNULL, RUN_TTY, RUN_REALLY);
		    
		if (retcode == -1)
		{
		    error (1, errno,
			   "fork failed while examining conflict in `%s'",
			   finfo->fullname);
		}
		else if (retcode == 0)
		{
		    error (0, 0,
			   "file `%s' still contains conflict indicators",
			   finfo->fullname);
		    freevers_ts (&vers);
		    return (1);
		}
	    }

	    if (status == T_REMOVED && vers->tag && isdigit (*vers->tag))
	    {
		error (0, 0,
	"cannot remove file `%s' which has a numeric sticky tag of `%s'",
			   finfo->fullname, vers->tag);
		freevers_ts (&vers);
		return (1);
	    }
	    if (status == T_ADDED)
	    {
		char rcs[PATH_MAX];

		/* Don't look in the attic; if it exists there we will
		   move it back out in checkaddfile.  */
		sprintf(rcs, "%s/%s%s", finfo->repository, finfo->file, RCSEXT);
		if (isreadable (rcs))
		{
		    error (0, 0,
		"cannot add file `%s' when RCS file `%s' already exists",
			   finfo->fullname, rcs);
		    freevers_ts (&vers);
		    return (1);
		}
		if (vers->tag && isdigit (*vers->tag) &&
		    numdots (vers->tag) > 1)
		{
		    error (0, 0,
		"cannot add file `%s' with revision `%s'; must be on trunk",
			       finfo->fullname, vers->tag);
		    freevers_ts (&vers);
		    return (1);
		}
	    }

	    /* done with consistency checks; now, to get on with the commit */
	    if (finfo->update_dir[0] == '\0')
		xdir = ".";
	    else
		xdir = finfo->update_dir;
	    if ((p = findnode (mulist, xdir)) != NULL)
	    {
		ulist = ((struct master_lists *) p->data)->ulist;
		cilist = ((struct master_lists *) p->data)->cilist;
	    }
	    else
	    {
		struct master_lists *ml;

		ulist = getlist ();
		cilist = getlist ();
		p = getnode ();
		p->key = xstrdup (xdir);
		p->type = UPDATE;
		ml = (struct master_lists *)
		    xmalloc (sizeof (struct master_lists));
		ml->ulist = ulist;
		ml->cilist = cilist;
		p->data = (char *) ml;
		p->delproc = masterlist_delproc;
		(void) addnode (mulist, p);
	    }

	    /* first do ulist, then cilist */
	    p = getnode ();
	    p->key = xstrdup (finfo->file);
	    p->type = UPDATE;
	    p->delproc = update_delproc;
	    p->data = (char *) status;
	    (void) addnode (ulist, p);

	    p = getnode ();
	    p->key = xstrdup (finfo->file);
	    p->type = UPDATE;
	    p->delproc = ci_delproc;
	    ci = (struct commit_info *) xmalloc (sizeof (struct commit_info));
	    ci->status = status;
	    if (vers->tag)
		if (isdigit (*vers->tag))
		    ci->rev = xstrdup (vers->tag);
		else
		    ci->rev = RCS_whatbranch (finfo->rcs, vers->tag);
	    else
		ci->rev = (char *) NULL;
	    ci->tag = xstrdup (vers->tag);
	    ci->options = xstrdup(vers->options);
	    p->data = (char *) ci;
	    (void) addnode (cilist, p);
	    break;
	case T_UNKNOWN:
	    error (0, 0, "nothing known about `%s'", finfo->fullname);
	    freevers_ts (&vers);
	    return (1);
	case T_UPTODATE:
	    break;
	default:
	    error (0, 0, "CVS internal error: unknown status %d", status);
	    break;
    }

    freevers_ts (&vers);
    return (0);
}

/*
 * Print warm fuzzies while examining the dirs
 */
/* ARGSUSED */
static Dtype
check_direntproc (dir, repos, update_dir)
    char *dir;
    char *repos;
    char *update_dir;
{
    if (!quiet)
	error (0, 0, "Examining %s", update_dir);

    return (R_PROCESS);
}

/*
 * Walklist proc to run pre-commit checks
 */
static int
precommit_list_proc (p, closure)
    Node *p;
    void *closure;
{
    if (p->data == (char *) T_ADDED || p->data == (char *) T_MODIFIED ||
	p->data == (char *) T_REMOVED)
    {
	run_arg (p->key);
    }
    return (0);
}

/*
 * Callback proc for pre-commit checking
 */
static List *ulist;
static int
precommit_proc (repository, filter)
    char *repository;
    char *filter;
{
    /* see if the filter is there, only if it's a full path */
    if (isabsolute (filter))
    {
    	char *s, *cp;
	
	s = xstrdup (filter);
	for (cp = s; *cp; cp++)
	    if (isspace (*cp))
	    {
		*cp = '\0';
		break;
	    }
	if (!isfile (s))
	{
	    error (0, errno, "cannot find pre-commit filter `%s'", s);
	    free (s);
	    return (1);			/* so it fails! */
	}
	free (s);
    }

    run_setup ("%s %s", filter, repository);
    (void) walklist (ulist, precommit_list_proc, NULL);
    return (run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL|RUN_REALLY));
}

/*
 * Run the pre-commit checks for the dir
 */
/* ARGSUSED */
static int
check_filesdoneproc (err, repos, update_dir)
    int err;
    char *repos;
    char *update_dir;
{
    int n;
    Node *p;

    /* find the update list for this dir */
    p = findnode (mulist, update_dir);
    if (p != NULL)
	ulist = ((struct master_lists *) p->data)->ulist;
    else
	ulist = (List *) NULL;

    /* skip the checks if there's nothing to do */
    if (ulist == NULL || ulist->list->next == ulist->list)
	return (err);

    /* run any pre-commit checks */
    if ((n = Parse_Info (CVSROOTADM_COMMITINFO, repos, precommit_proc, 1)) > 0)
    {
	error (0, 0, "Pre-commit check failed");
	err += n;
    }

    return (err);
}

/*
 * Do the work of committing a file
 */
static int maxrev;
static char sbranch[PATH_MAX];

/* ARGSUSED */
static int
commit_fileproc (finfo)
    struct file_info *finfo;
{
    Node *p;
    int err = 0;
    List *ulist, *cilist;
    struct commit_info *ci;
    char rcs[PATH_MAX];

    if (finfo->update_dir[0] == '\0')
	p = findnode (mulist, ".");
    else
	p = findnode (mulist, finfo->update_dir);

    /*
     * if p is null, there were file type command line args which were
     * all up-to-date so nothing really needs to be done
     */
    if (p == NULL)
	return (0);
    ulist = ((struct master_lists *) p->data)->ulist;
    cilist = ((struct master_lists *) p->data)->cilist;

    /*
     * At this point, we should have the commit message unless we were called
     * with files as args from the command line.  In that latter case, we
     * need to get the commit message ourselves
     */
    if (use_editor && !got_message)
      {
	got_message = 1;
	do_editor (finfo->update_dir, &message, finfo->repository, ulist);
      }

    p = findnode (cilist, finfo->file);
    if (p == NULL)
	return (0);

    ci = (struct commit_info *) p->data;
    if (ci->status == T_MODIFIED)
    {
	if (lockrcsfile (finfo->file, finfo->repository, ci->rev) != 0)
	{
	    unlockrcs (finfo->file, finfo->repository);
	    err = 1;
	    goto out;
	}
    }
    else if (ci->status == T_ADDED)
    {
	if (checkaddfile (finfo->file, finfo->repository, ci->tag, ci->options,
			  &finfo->rcs) != 0)
	{
	    fixaddfile (finfo->file, finfo->repository);
	    err = 1;
	    goto out;
	}

	/* adding files with a tag, now means adding them on a branch.
	   Since the branch test was done in check_fileproc for
	   modified files, we need to stub it in again here. */

	if (ci->tag) {
	    locate_rcs (finfo->file, finfo->repository, rcs);
	    ci->rev = RCS_whatbranch (finfo->rcs, ci->tag);
	    err = Checkin ('A', finfo->file, finfo->update_dir, finfo->repository, rcs, ci->rev,
			   ci->tag, ci->options, message, finfo->entries);
	    if (err != 0)
	    {
		unlockrcs (finfo->file, finfo->repository);
		fixbranch (finfo->file, finfo->repository, sbranch);
	    }

	    (void) time (&last_register_time);

	    ci->status = T_UPTODATE;
	}
    }

    /*
     * Add the file for real
     */
    if (ci->status == T_ADDED)
    {
	char *xrev = (char *) NULL;

	if (ci->rev == NULL)
	{
	    /* find the max major rev number in this directory */
	    maxrev = 0;
	    (void) walklist (finfo->entries, findmaxrev, NULL);
	    if (maxrev == 0)
		maxrev = 1;
	    xrev = xmalloc (20);
	    (void) sprintf (xrev, "%d", maxrev);
	}

	/* XXX - an added file with symbolic -r should add tag as well */
	err = finaladd (finfo->file, ci->rev ? ci->rev : xrev, ci->tag, ci->options,
			finfo->update_dir, finfo->repository, finfo->entries);
	if (xrev)
	    free (xrev);
    }
    else if (ci->status == T_MODIFIED)
    {
	locate_rcs (finfo->file, finfo->repository, rcs);
	err = Checkin ('M', finfo->file, finfo->update_dir, finfo->repository,
		       rcs, ci->rev, ci->tag,
		       ci->options, message, finfo->entries);

	(void) time (&last_register_time);

	if (err != 0)
	{
	    unlockrcs (finfo->file, finfo->repository);
	    fixbranch (finfo->file, finfo->repository, sbranch);
	}
    }
    else if (ci->status == T_REMOVED)
    {
	err = remove_file (finfo->file, finfo->repository, ci->tag, message,
			   finfo->entries, finfo->rcs);
#ifdef SERVER_SUPPORT
	if (server_active) {
	    server_scratch_entry_only ();
	    server_updated (finfo->file, finfo->update_dir, finfo->repository,
			    /* Doesn't matter, it won't get checked.  */
			    SERVER_UPDATED, (struct stat *) NULL,
			    (unsigned char *) NULL);
	}
#endif
    }

    /* Clearly this is right for T_MODIFIED.  I haven't thought so much
       about T_ADDED or T_REMOVED.  */
    notify_do ('C', finfo->file, getcaller (), NULL, NULL, finfo->repository);

out:
    if (err != 0)
    {
	/* on failure, remove the file from ulist */
	p = findnode (ulist, finfo->file);
	if (p)
	    delnode (p);
    }

    return (err);
}

/*
 * Log the commit and clean up the update list
 */
/* ARGSUSED */
static int
commit_filesdoneproc (err, repository, update_dir)
    int err;
    char *repository;
    char *update_dir;
{
    char *xtag = (char *) NULL;
    Node *p;
    List *ulist;

    p = findnode (mulist, update_dir);
    if (p == NULL)
	return (err);

    ulist = ((struct master_lists *) p->data)->ulist;

    got_message = 0;

    /* see if we need to specify a per-directory or -r option tag */
    if (tag == NULL)
	ParseTag (&xtag, (char **) NULL);

    Update_Logfile (repository, message, tag ? tag : xtag, (FILE *) 0, ulist);
    if (xtag)
	free (xtag);

    /* Build the administrative files if necessary.  */
    {
	char *p;

	if (strncmp (CVSroot, repository, strlen (CVSroot)) != 0)
	    error (0, 0, "internal error: repository doesn't begin with root");
	p = repository + strlen (CVSroot);
	if (*p == '/')
	    ++p;
	if (strcmp ("CVSROOT", p) == 0)
	{
	    /* "Database" might a little bit grandiose and/or vague,
	       but "checked-out copies of administrative files, unless
	       in the case of modules and you are using ndbm in which
	       case modules.{pag,dir,db}" is verbose and excessively
	       focused on how the database is implemented.  */

	    cvs_output (program_name, 0);
	    cvs_output (" ", 1);
	    cvs_output (command_name, 0);
	    cvs_output (": Rebuilding administrative file database\n", 0);
	    mkmodules (repository);
	}
    }

    if (err == 0 && run_module_prog)
    {
	FILE *fp;

	if ((fp = fopen (CVSADM_CIPROG, "r")) != NULL)
	{
	    char *line;
	    int line_length;
	    size_t line_chars_allocated;
	    char *repository;

	    line = NULL;
	    line_chars_allocated = 0;
	    line_length = getline (&line, &line_chars_allocated, fp);
	    if (line_length > 0)
	    {
		/* Remove any trailing newline.  */
		if (line[line_length - 1] == '\n')
		    line[--line_length] = '\0';
		repository = Name_Repository ((char *) NULL, update_dir);
		run_setup ("%s %s", line, repository);
		(void) printf ("%s %s: Executing '", program_name,
			       command_name);
		run_print (stdout);
		(void) printf ("'\n");
		(void) run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL);
		free (repository);
	    }
	    else
	    {
		if (ferror (fp))
		    error (0, errno, "warning: error reading %s",
			   CVSADM_CIPROG);
	    }
	    if (line != NULL)
		free (line);
	    if (fclose (fp) < 0)
		error (0, errno, "warning: cannot close %s", CVSADM_CIPROG);
	}
	else
	{
	    if (! existence_error (errno))
		error (0, errno, "warning: cannot open %s", CVSADM_CIPROG);
	}
    }

    return (err);
}

/*
 * Get the log message for a dir and print a warm fuzzy
 */
/* ARGSUSED */
static Dtype
commit_direntproc (dir, repos, update_dir)
    char *dir;
    char *repos;
    char *update_dir;
{
    Node *p;
    List *ulist;
    char *real_repos;

    /* find the update list for this dir */
    p = findnode (mulist, update_dir);
    if (p != NULL)
	ulist = ((struct master_lists *) p->data)->ulist;
    else
	ulist = (List *) NULL;

    /* skip the files as an optimization */
    if (ulist == NULL || ulist->list->next == ulist->list)
	return (R_SKIP_FILES);

    /* print the warm fuzzy */
    if (!quiet)
	error (0, 0, "Committing %s", update_dir);

    /* get commit message */
    if (use_editor)
    {
	got_message = 1;
	real_repos = Name_Repository (dir, update_dir);
	do_editor (update_dir, &message, real_repos, ulist);
	free (real_repos);
    }
    return (R_PROCESS);
}

/*
 * Process the post-commit proc if necessary
 */
/* ARGSUSED */
static int
commit_dirleaveproc (dir, err, update_dir)
    char *dir;
    int err;
    char *update_dir;
{
    /* update the per-directory tag info */
    if (err == 0 && write_dirtag != NULL)
    {
	WriteTag ((char *) NULL, write_dirtag, (char *) NULL);
#ifdef SERVER_SUPPORT
	if (server_active)
	    server_set_sticky (update_dir, Name_Repository (dir, update_dir),
			       write_dirtag, (char *) NULL);
#endif
    }

    return (err);
}

/*
 * find the maximum major rev number in an entries file
 */
static int
findmaxrev (p, closure)
    Node *p;
    void *closure;
{
    char *cp;
    int thisrev;
    Entnode *entdata;

    entdata = (Entnode *) p->data;
    cp = strchr (entdata->version, '.');
    if (cp != NULL)
	*cp = '\0';
    thisrev = atoi (entdata->version);
    if (cp != NULL)
	*cp = '.';
    if (thisrev > maxrev)
	maxrev = thisrev;
    return (0);
}

/*
 * Actually remove a file by moving it to the attic
 * XXX - if removing a ,v file that is a relative symbolic link to
 * another ,v file, we probably should add a ".." component to the
 * link to keep it relative after we move it into the attic.
 */
static int
remove_file (file, repository, tag, message, entries, rcsnode)
    char *file;
    char *repository;
    char *tag;
    char *message;
    List *entries;
    RCSNode *rcsnode;
{
    mode_t omask;
    int retcode;
    char rcs[PATH_MAX];
    char *tmp;

    int branch;
    int lockflag;
    char *corev;
    char *rev;
    char *prev_rev;

    corev = NULL;
    rev = NULL;
    prev_rev = NULL;

    retcode = 0;

    locate_rcs (file, repository, rcs);

    branch = 0;
    if (tag && !(branch = RCS_isbranch (rcsnode, tag)))
    {
	/* a symbolic tag is specified; just remove the tag from the file */
	if ((retcode = RCS_deltag (rcs, tag, 1)) != 0) 
	{
	    if (!quiet)
		error (0, retcode == -1 ? errno : 0,
		       "failed to remove tag `%s' from `%s'", tag, rcs);
	    return (1);
	}
	Scratch_Entry (entries, file);
	return (0);
    }

    /* we are removing the file from either the head or a branch */
    /* commit a new, dead revision. */

    /* Print message indicating that file is going to be removed. */
    (void) printf ("Removing %s;\n", file);

    rev = NULL;
    lockflag = RCS_FLAGS_LOCK;
    if (branch)
    {
	char *branchname;

	rev = RCS_whatbranch (rcsnode, tag);
	if (rev == NULL)
	{
	    error (0, 0, "cannot find branch \"%s\".", tag);
	    return (1);
	}
	
	if (rcsnode == NULL)
	{
	    error (0, 0, "boy, I'm confused.");
	    return (1);
	}
	branchname = RCS_getbranch (rcsnode, rev, 1);
	if (branchname == NULL)
	{
	    /* no revision exists on this branch.  use the previous
	       revision but do not lock. */
	    corev = RCS_gettag (rcsnode, tag, 1, 0);
	    prev_rev = xstrdup(rev);
	    lockflag = 0;
	} else
	{
	    corev = xstrdup (rev);
	    prev_rev = xstrdup(branchname);
	    free (branchname);
	}

    } else  /* Not a branch */
    {

        /* Get current head revision of file. */
	if (rcsnode == NULL)
	{
	    error (0, 0, "could not find parsed rcsfile %s", file);
	    return (1);
	}
	prev_rev = RCS_head (rcsnode);
    }
    
    /* if removing without a tag or a branch, then make sure the default
       branch is the trunk. */
    if (!tag && !branch)
    {
        if (RCS_setbranch (rcs, NULL) != 0) 
	{
	    error (0, 0, "cannot change branch to default for %s",
		   rcs);
	    return (1);
	}
    }

#ifdef SERVER_SUPPORT
    if (server_active) {
	/* If this is the server, there will be a file sitting in the
	   temp directory which is the kludgy way in which server.c
	   tells time_stamp that the file is no longer around.  Remove
	   it so we can create temp files with that name (ignore errors).  */
	unlink_file (file);
    }
#endif

    /* check something out.  Generally this is the head.  If we have a
       particular rev, then name it.  except when creating a branch,
       lock the rev we're checking out.  */
    retcode = RCS_checkout (rcs, "", rev ? corev : NULL, NULL, RUN_TTY,
                            lockflag, 1);
    if (retcode != 0)
    {
	if (!quiet)
	    error (0, retcode == -1 ? errno : 0,
		   "failed to check out `%s'", rcs);
	return (1);
    }

    if (corev != NULL)
	free (corev);

    retcode = RCS_checkin (rcs, NULL, message, rev, RCS_FLAGS_DEAD, 1);
    if (retcode	!= 0)
    {
	if (!quiet)
	    error (0, retcode == -1 ? errno : 0,
		   "failed to commit dead revision for `%s'", rcs);
	return (1);
    }

    if (rev != NULL)
	free (rev);

    if (!branch)
    {
	/* this was the head; really move it into the Attic */
	tmp = xmalloc(strlen(repository) + 
		      sizeof('/') +
		      sizeof(CVSATTIC) +
		      sizeof('/') +
		      strlen(file) +
		      sizeof(RCSEXT) + 1);
	(void) sprintf (tmp, "%s/%s", repository, CVSATTIC);
	omask = umask (cvsumask);
	(void) CVS_MKDIR (tmp, 0777);
	(void) umask (omask);
	(void) sprintf (tmp, "%s/%s/%s%s", repository, CVSATTIC, file, RCSEXT);
	
	if (strcmp (rcs, tmp) != 0
	    && rename (rcs, tmp) == -1
	    && (isreadable (rcs) || !isreadable (tmp)))
	{
	    free(tmp);
	    return (1);
	}
	free(tmp);
    }

    /* Print message that file was removed. */
    (void) printf ("%s  <--  %s\n", rcs, file);
    (void) printf ("new revision: delete; ");
    (void) printf ("previous revision: %s\n", prev_rev);
    (void) printf ("done\n");
    free(prev_rev);

    Scratch_Entry (entries, file);
    return (0);
}

/*
 * Do the actual checkin for added files
 */
static int
finaladd (file, rev, tag, options, update_dir, repository, entries)
    char *file;
    char *rev;
    char *tag;
    char *options;
    char *update_dir;
    char *repository;
    List *entries;
{
    int ret;
    char tmp[PATH_MAX];
    char rcs[PATH_MAX];

    locate_rcs (file, repository, rcs);
    ret = Checkin ('A', file, update_dir, repository, rcs, rev, tag, options,
		   message, entries);
    if (ret == 0)
    {
	(void) sprintf (tmp, "%s/%s%s", CVSADM, file, CVSEXT_LOG);
	(void) unlink_file (tmp);
    }
    else
	fixaddfile (file, repository);

    (void) time (&last_register_time);

    return (ret);
}

/*
 * Unlock an rcs file
 */
static void
unlockrcs (file, repository)
    char *file;
    char *repository;
{
    char rcs[PATH_MAX];
    int retcode = 0;

    locate_rcs (file, repository, rcs);

    if ((retcode = RCS_unlock (rcs, NULL, 0)) != 0)
	error (retcode == -1 ? 1 : 0, retcode == -1 ? errno : 0,
	       "could not unlock %s", rcs);
}

/*
 * remove a partially added file.  if we can parse it, leave it alone.
 */
static void
fixaddfile (file, repository)
    char *file;
    char *repository;
{
    RCSNode *rcsfile;
    char rcs[PATH_MAX];
    int save_really_quiet;

    locate_rcs (file, repository, rcs);
    save_really_quiet = really_quiet;
    really_quiet = 1;
    if ((rcsfile = RCS_parsercsfile (rcs)) == NULL)
	(void) unlink_file (rcs);
    else
	freercsnode (&rcsfile);
    really_quiet = save_really_quiet;
}

/*
 * put the branch back on an rcs file
 */
static void
fixbranch (file, repository, branch)
    char *file;
    char *repository;
    char *branch;
{
    char rcs[PATH_MAX];
    int retcode = 0;

    if (branch != NULL && branch[0] != '\0')
    {
	locate_rcs (file, repository, rcs);
	if ((retcode = RCS_setbranch (rcs, branch)) != 0)
	    error (retcode == -1 ? 1 : 0, retcode == -1 ? errno : 0,
		   "cannot restore branch to %s for %s", branch, rcs);
    }
}

/*
 * do the initial part of a file add for the named file.  if adding
 * with a tag, put the file in the Attic and point the symbolic tag
 * at the committed revision.
 */

static int
checkaddfile (file, repository, tag, options, rcsnode)
    char *file;
    char *repository;
    char *tag;
    char *options;
    RCSNode **rcsnode;
{
    char rcs[PATH_MAX];
    char fname[PATH_MAX];
    mode_t omask;
    int retcode = 0;
    int newfile = 0;

    if (tag)
    {
	(void) sprintf(rcs, "%s/%s", repository, CVSATTIC);
	omask = umask (cvsumask);
	if (CVS_MKDIR (rcs, 0777) != 0 && errno != EEXIST)
	    error (1, errno, "cannot make directory `%s'", rcs);;
	(void) umask (omask);
	(void) sprintf (rcs, "%s/%s/%s%s", repository, CVSATTIC, file, RCSEXT);
    }
    else
	locate_rcs (file, repository, rcs);

    if (isreadable(rcs))
    {
	/* file has existed in the past.  Prepare to resurrect. */
	char oldfile[PATH_MAX];
	char *rev;
	RCSNode *rcsfile;

	if (tag == NULL)
	{
	    /* we are adding on the trunk, so move the file out of the
	       Attic. */
	    strcpy (oldfile, rcs);
	    sprintf (rcs, "%s/%s%s", repository, file, RCSEXT);
	    
	    if (strcmp (oldfile, rcs) == 0
		|| rename (oldfile, rcs) != 0
		|| isreadable (oldfile)
		|| !isreadable (rcs))
	    {
		error (0, 0, "failed to move `%s' out of the attic.",
		       file);
		return (1);
	    }
	}

	if ((rcsfile = *rcsnode) == NULL)
	{
	    error (0, 0, "could not find parsed rcsfile %s", file);
	    return (1);
	}

	rev = RCS_getversion (rcsfile, tag, NULL, 1, 0);
	/* and lock it */
	if (lock_RCS (file, rcs, rev, repository)) {
	    error (0, 0, "cannot lock `%s'.", rcs);
	    free (rev);
	    return (1);
	}

	free (rev);
    } else {
	/* this is the first time we have ever seen this file; create
	   an rcs file.  */
	run_setup ("%s%s -x,v/ -i", Rcsbin, RCS);

	(void) sprintf (fname, "%s/%s%s", CVSADM, file, CVSEXT_LOG);
	/* If the file does not exist, no big deal.  In particular, the
	   server does not (yet at least) create CVSEXT_LOG files.  */
	if (isfile (fname))
	    run_args ("-t%s/%s%s", CVSADM, file, CVSEXT_LOG);

	/* Set RCS keyword expansion options.  */
	if (options && options[0] == '-' && options[1] == 'k')
	    run_arg (options);
	run_arg (rcs);
	if ((retcode = run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL)) != 0)
	{
	    error (retcode == -1 ? 1 : 0, retcode == -1 ? errno : 0,
		   "could not create %s", rcs);
	    return (1);
	}
	newfile = 1;
    }

    /* when adding a file for the first time, and using a tag, we need
       to create a dead revision on the trunk.  */
    if (tag && newfile)
    {
	char *tmp;

	/* move the new file out of the way. */
	(void) sprintf (fname, "%s/%s%s", CVSADM, CVSPREFIX, file);
	rename_file (file, fname);
	copy_file (DEVNULL, file);

	tmp = xmalloc (strlen (file) + strlen (tag) + 80);
	/* commit a dead revision. */
	(void) sprintf (tmp, "file %s was initially added on branch %s.",
			file, tag);
	retcode = RCS_checkin (rcs, NULL, tmp, NULL,
			       RCS_FLAGS_DEAD | RCS_FLAGS_QUIET, 0);
	free (tmp);
	if (retcode != 0)
	{
	    error (retcode == -1 ? 1 : 0, retcode == -1 ? errno : 0,
		   "could not create initial dead revision %s", rcs);
	    return (1);
	}

	/* put the new file back where it was */
	rename_file (fname, file);
	
	/* and lock it once again. */
	if (lock_RCS (file, rcs, NULL, repository)) {
	    error (0, 0, "cannot lock `%s'.", rcs);
	    return (1);
	}
    }

    if (tag != NULL)
    {
	/* when adding with a tag, we need to stub a branch, if it
	   doesn't already exist.  */
	RCSNode *rcsfile;

	rcsfile = RCS_parse (file, repository);
	if (rcsfile == NULL)
	{
	    error (0, 0, "could not read %s", rcs);
	    return (1);
	}
	
	if (!RCS_nodeisbranch (rcsfile, tag)) {
	    /* branch does not exist.  Stub it.  */
	    char *head;
	    char *magicrev;
	    
	    head = RCS_getversion (rcsfile, NULL, NULL, 0, 0);
	    magicrev = RCS_magicrev (rcsfile, head);
	    if ((retcode = RCS_settag(rcs, tag, magicrev)) != 0)
	    {
		error (retcode == -1 ? 1 : 0, retcode == -1 ? errno : 0,
		       "could not stub branch %s for %s", tag, rcs);
		return (1);
	    }
	    
	    freercsnode (&rcsfile);
	    
	    /* reparse the file, then add it to our list. */
	    rcsfile = RCS_parse (file, repository);
	    if (rcsfile == NULL)
	    {
		error (0, 0, "could not reparse %s", rcs);
		return (1);
	    }

	    free (head);
	    free (magicrev);
	}
	else
	{
	    /* lock the branch. (stubbed branches need not be locked.)  */
	    if (lock_RCS (file, rcs, NULL, repository)) {
		error (0, 0, "cannot lock `%s'.", rcs);
		return (1);
	    }
	} 

	if (rcsnode)
		freercsnode(rcsnode);
	*rcsnode = rcsfile;
    }

    fileattr_newfile (file);

    fix_rcs_modes (rcs, file);
    return (0);
}

/*
 * Lock the rcs file ``file''
 */
static int
lockrcsfile (file, repository, rev)
    char *file;
    char *repository;
    char *rev;
{
    char rcs[PATH_MAX];

    locate_rcs (file, repository, rcs);
    if (lock_RCS (file, rcs, rev, repository) != 0)
	return (1);
    else
	return (0);
}

/*
 * Attempt to place a lock on the RCS file; returns 0 if it could and 1 if it
 * couldn't.  If the RCS file currently has a branch as the head, we must
 * move the head back to the trunk before locking the file, and be sure to
 * put the branch back as the head if there are any errors.
 */
static int
lock_RCS (user, rcs, rev, repository)
    char *user;
    char *rcs;
    char *rev;
    char *repository;
{
    RCSNode *rcsfile;
    char *branch = NULL;
    int err = 0;

    /*
     * For a specified, numeric revision of the form "1" or "1.1", (or when
     * no revision is specified ""), definitely move the branch to the trunk
     * before locking the RCS file.
     * 
     * The assumption is that if there is more than one revision on the trunk,
     * the head points to the trunk, not a branch... and as such, it's not
     * necessary to move the head in this case.
     */
    if (rev == NULL || (rev && isdigit (*rev) && numdots (rev) < 2))
    {
	if ((rcsfile = RCS_parsercsfile (rcs)) == NULL)
	{
	    /* invalid rcs file? */
	    err = 1;
	}
	else
	{
	    /* rcsfile is valid */
	    branch = xstrdup (rcsfile->branch);
	    freercsnode (&rcsfile);
	    if (branch != NULL)
	    {
		if (RCS_setbranch (rcs, NULL) != 0)
		{
		    error (0, 0, "cannot change branch to default for %s",
			   rcs);
		    if (branch)
			free (branch);
		    return (1);
		}
	    }
	    err = RCS_lock(rcs, NULL, 0);
	}
    }
    else
    {
	(void) RCS_lock(rcs, rev, 1);
    }

    if (err == 0)
    {
	if (branch)
	{
	    (void) strcpy (sbranch, branch);
	    free (branch);
	}
	else
	    sbranch[0] = '\0';
	return (0);
    }

    /* try to restore the branch if we can on error */
    if (branch != NULL)
	fixbranch (user, repository, branch);

    if (branch)
	free (branch);
    return (1);
}

/*
 * Called when "add"ing files to the RCS respository, as it is necessary to
 * preserve the file modes in the same fashion that RCS does.  This would be
 * automatic except that we are placing the RCS ,v file very far away from
 * the user file, and I can't seem to convince RCS of the location of the
 * user file.  So we munge it here, after the ,v file has been successfully
 * initialized with "rcs -i".
 */
static void
fix_rcs_modes (rcs, user)
    char *rcs;
    char *user;
{
    struct stat sb;

    if (stat (user, &sb) != -1)
	(void) chmod (rcs, (int) sb.st_mode & ~0222);
}

/*
 * free an UPDATE node's data (really nothing to do)
 */
void
update_delproc (p)
    Node *p;
{
    p->data = (char *) NULL;
}

/*
 * Free the commit_info structure in p.
 */
static void
ci_delproc (p)
    Node *p;
{
    struct commit_info *ci;

    ci = (struct commit_info *) p->data;
    if (ci->rev)
	free (ci->rev);
    if (ci->tag)
	free (ci->tag);
    if (ci->options)
	free (ci->options);
    free (ci);
}

/*
 * Free the commit_info structure in p.
 */
static void
masterlist_delproc (p)
    Node *p;
{
    struct master_lists *ml;

    ml = (struct master_lists *) p->data;
    dellist (&ml->ulist);
    dellist (&ml->cilist);
    free (ml);
}

/*
 * Find an RCS file in the repository.
 */
static void
locate_rcs (file, repository, rcs)
    char *file;
    char *repository;
    char *rcs;
{
    (void) sprintf (rcs, "%s/%s%s", repository, file, RCSEXT);
    if (!isreadable (rcs))
    {
	(void) sprintf (rcs, "%s/%s/%s%s", repository, CVSATTIC, file, RCSEXT);
	if (!isreadable (rcs))
	    (void) sprintf (rcs, "%s/%s%s", repository, file, RCSEXT);
    }
}
