/*
 * Copyright (c) 1992, Brian Berliner and Jeff Polk
 * Copyright (c) 1989-1992, Brian Berliner
 * 
 * You may distribute under the terms of the GNU General Public License as
 * specified in the README file that comes with the CVS 1.4 kit.
 * 
 * Status Information
 */

#include "cvs.h"

static Dtype status_dirproc PROTO((char *dir, char *repos, char *update_dir));
static int status_fileproc PROTO((struct file_info *finfo));
static int tag_list_proc PROTO((Node * p, void *closure));

static int local = 0;
static int long_format = 0;
static RCSNode *xrcsnode;

static const char *const status_usage[] =
{
    "Usage: %s %s [-vlR] [files...]\n",
    "\t-v\tVerbose format; includes tag information for the file\n",
    "\t-l\tProcess this directory only (not recursive).\n",
    "\t-R\tProcess directories recursively.\n",
    NULL
};

int
status (argc, argv)
    int argc;
    char **argv;
{
    int c;
    int err = 0;

    if (argc == -1)
	usage (status_usage);

    optind = 1;
    while ((c = getopt (argc, argv, "vlR")) != -1)
    {
	switch (c)
	{
	    case 'v':
		long_format = 1;
		break;
	    case 'l':
		local = 1;
		break;
	    case 'R':
		local = 0;
		break;
	    case '?':
	    default:
		usage (status_usage);
		break;
	}
    }
    argc -= optind;
    argv += optind;

    wrap_setup ();

#ifdef CLIENT_SUPPORT
    if (client_active) {
      start_server ();

      ign_setup ();

      if (long_format)
	send_arg("-v");
      if (local)
	send_arg("-l");

      send_file_names (argc, argv, SEND_EXPAND_WILD);
      /* XXX This should only need to send file info; the file
	 contents themselves will not be examined.  */
      send_files (argc, argv, local, 0);

      send_to_server ("status\012", 0);
      err = get_responses_and_close ();

      return err;
    }
#endif

    /* start the recursion processor */
    err = start_recursion (status_fileproc, (FILESDONEPROC) NULL, status_dirproc,
			   (DIRLEAVEPROC) NULL, argc, argv, local,
			   W_LOCAL, 0, 1, (char *) NULL, 1, 0);

    return (err);
}

/*
 * display the status of a file
 */
/* ARGSUSED */
static int
status_fileproc (finfo)
    struct file_info *finfo;
{
    Ctype status;
    char *sstat;
    Vers_TS *vers;

    status = Classify_File (finfo->file, (char *) NULL, (char *) NULL, (char *) NULL,
			    1, 0, finfo->repository, finfo->entries, finfo->rcs, &vers,
			    finfo->update_dir, 0);
    switch (status)
    {
	case T_UNKNOWN:
	    sstat = "Unknown";
	    break;
	case T_CHECKOUT:
	    sstat = "Needs Checkout";
	    break;
#ifdef SERVER_SUPPORT
	case T_PATCH:
	    sstat = "Needs Patch";
	    break;
#endif
	case T_CONFLICT:
	    sstat = "Unresolved Conflict";
	    break;
	case T_ADDED:
	    sstat = "Locally Added";
	    break;
	case T_REMOVED:
	    sstat = "Locally Removed";
	    break;
	case T_MODIFIED:
	    if (vers->ts_conflict)
		sstat = "Unresolved Conflict";
	    else
		sstat = "Locally Modified";
	    break;
	case T_REMOVE_ENTRY:
	    sstat = "Entry Invalid";
	    break;
	case T_UPTODATE:
	    sstat = "Up-to-date";
	    break;
	case T_NEEDS_MERGE:
	    sstat = "Needs Merge";
	    break;
	default:
	    sstat = "Classify Error";
	    break;
    }

    (void) printf ("===================================================================\n");
    if (vers->ts_user == NULL)
	(void) printf ("File: no file %s\t\tStatus: %s\n\n", finfo->file, sstat);
    else
	(void) printf ("File: %-17s\tStatus: %s\n\n", finfo->file, sstat);

    if (vers->vn_user == NULL)
	(void) printf ("   Working revision:\tNo entry for %s\n", finfo->file);
    else if (vers->vn_user[0] == '0' && vers->vn_user[1] == '\0')
	(void) printf ("   Working revision:\tNew file!\n");
#ifdef SERVER_SUPPORT
    else if (server_active)
	(void) printf ("   Working revision:\t%s\n", vers->vn_user);
#endif
    else
	(void) printf ("   Working revision:\t%s\t%s\n", vers->vn_user,
		       vers->ts_rcs);

    if (vers->vn_rcs == NULL)
	(void) printf ("   Repository revision:\tNo revision control file\n");
    else
	(void) printf ("   Repository revision:\t%s\t%s\n", vers->vn_rcs,
		       vers->srcfile->path);

    if (vers->entdata)
    {
	Entnode *edata;

	edata = vers->entdata;
	if (edata->tag)
	{
	    if (vers->vn_rcs == NULL)
		(void) printf (
			 "   Sticky Tag:\t\t%s - MISSING from RCS file!\n",
			 edata->tag);
	    else
	    {
		if (isdigit (edata->tag[0]))
		    (void) printf ("   Sticky Tag:\t\t%s\n", edata->tag);
		else
		{
		    char *branch = NULL;
	
		    if (RCS_isbranch (finfo->rcs, edata->tag))
			branch = RCS_whatbranch(finfo->rcs, edata->tag);

		    (void) printf ("   Sticky Tag:\t\t%s (%s: %s)\n",
				   edata->tag,
				   branch ? "branch" : "revision",
				   branch ? branch : vers->vn_rcs);

		    if (branch)
			free (branch);
		}
	    }
	}
	else if (!really_quiet)
	    (void) printf ("   Sticky Tag:\t\t(none)\n");

	if (edata->date)
	    (void) printf ("   Sticky Date:\t\t%s\n", edata->date);
	else if (!really_quiet)
	    (void) printf ("   Sticky Date:\t\t(none)\n");

	if (edata->options && edata->options[0])
	    (void) printf ("   Sticky Options:\t%s\n", edata->options);
	else if (!really_quiet)
	    (void) printf ("   Sticky Options:\t(none)\n");

	if (long_format && vers->srcfile)
	{
	    List *symbols = RCS_symbols(vers->srcfile);

	    (void) printf ("\n   Existing Tags:\n");
	    if (symbols)
	    {
		xrcsnode = finfo->rcs;
		(void) walklist (symbols, tag_list_proc, NULL);
	    }
	    else
		(void) printf ("\tNo Tags Exist\n");
	}
    }

    (void) printf ("\n");
    freevers_ts (&vers);
    return (0);
}

/*
 * Print a warm fuzzy message
 */
/* ARGSUSED */
static Dtype
status_dirproc (dir, repos, update_dir)
    char *dir;
    char *repos;
    char *update_dir;
{
    if (!quiet)
	error (0, 0, "Examining %s", update_dir);
    return (R_PROCESS);
}

/*
 * Print out a tag and its type
 */
static int
tag_list_proc (p, closure)
    Node *p;
    void *closure;
{
    char *branch = NULL;

    if (RCS_isbranch (xrcsnode, p->key))
	branch = RCS_whatbranch(xrcsnode, p->key) ;

    (void) printf ("\t%-25.25s\t(%s: %s)\n", p->key,
		   branch ? "branch" : "revision",
		   branch ? branch : p->data);

    if (branch)
	free (branch);

    return (0);
}
