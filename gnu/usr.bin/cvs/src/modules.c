/*
 *    Copyright (c) 1992, Brian Berliner and Jeff Polk
 *    Copyright (c) 1989-1992, Brian Berliner
 *
 *    You may distribute under the terms of the GNU General Public License
 *    as specified in the README file that comes with the CVS 1.4 kit.
 *
 * Modules
 *
 *	Functions for accessing the modules file.
 *
 *	The modules file supports basically three formats of lines:
 *		key [options] directory files... [ -x directory [files] ] ...
 *		key [options] directory [ -x directory [files] ] ...
 *		key -a aliases...
 *
 *	The -a option allows an aliasing step in the parsing of the modules
 *	file.  The "aliases" listed on a line following the -a are
 *	processed one-by-one, as if they were specified as arguments on the
 *	command line.
 */

#include "cvs.h"
#include "savecwd.h"


/* Defines related to the syntax of the modules file.  */

/* Options in modules file.  Note that it is OK to use GNU getopt features;
   we already are arranging to make sure we are using the getopt distributed
   with CVS.  */
#define	CVSMODULE_OPTS	"+ad:i:lo:e:s:t:u:"

/* Special delimiter.  */
#define CVSMODULE_SPEC	'&'

struct sortrec
{
    char *modname;
    char *status;
    char *rest;
    char *comment;
};

static int sort_order PROTO((const PTR l, const PTR r));
static void save_d PROTO((char *k, int ks, char *d, int ds));


/*
 * Open the modules file, and die if the CVSROOT environment variable
 * was not set.  If the modules file does not exist, that's fine, and
 * a warning message is displayed and a NULL is returned.
 */
DBM *
open_module ()
{
    char mfile[PATH_MAX];

    if (CVSroot_original == NULL)
    {
	(void) fprintf (stderr, 
			"%s: must set the CVSROOT environment variable\n",
			program_name);
	error (1, 0, "or specify the '-d' option to %s", program_name);
    }
    (void) sprintf (mfile, "%s/%s/%s", CVSroot_directory,
		    CVSROOTADM, CVSROOTADM_MODULES);
    return (dbm_open (mfile, O_RDONLY, 0666));
}

/*
 * Close the modules file, if the open succeeded, that is
 */
void
close_module (db)
    DBM *db;
{
    if (db != NULL)
	dbm_close (db);
}

/*
 * This is the recursive function that processes a module name.
 * It calls back the passed routine for each directory of a module
 * It runs the post checkout or post tag proc from the modules file
 */
int
do_module (db, mname, m_type, msg, callback_proc, where,
	   shorten, local_specified, run_module_prog, extra_arg)
    DBM *db;
    char *mname;
    enum mtype m_type;
    char *msg;
    CALLBACKPROC callback_proc;
    char *where;
    int shorten;
    int local_specified;
    int run_module_prog;
    char *extra_arg;
{
    char *checkin_prog = NULL;
    char *checkout_prog = NULL;
    char *export_prog = NULL;
    char *tag_prog = NULL;
    char *update_prog = NULL;
    struct saved_cwd cwd;
    char *line;
    int modargc;
    int xmodargc;
    char **modargv;
    char *xmodargv[MAXFILEPERDIR];
    char *value;
    char *zvalue;
    char *mwhere = NULL;
    char *mfile = NULL;
    char *spec_opt = NULL;
    char xvalue[PATH_MAX];
    int alias = 0;
    datum key, val;
    char *cp;
    int c, err = 0;

#ifdef SERVER_SUPPORT
    if (trace)
    {
	char *buf;

	/* We use cvs_outerr, rather than fprintf to stderr, because
	   this may be called by server code with error_use_protocol
	   set.  */
	buf = xmalloc (100
		       + strlen (mname)
		       + strlen (msg)
		       + (where ? strlen (where) : 0)
		       + (extra_arg ? strlen (extra_arg) : 0));
	sprintf (buf, "%c-> do_module (%s, %s, %s, %s)\n",
		 (server_active) ? 'S' : ' ',
		 mname, msg, where ? where : "",
		 extra_arg ? extra_arg : "");
	cvs_outerr (buf, 0);
	free (buf);
    }
#endif

    /* if this is a directory to ignore, add it to that list */
    if (mname[0] == '!' && mname[1] != '\0')
    {
	ign_dir_add (mname+1);
	return(err);
    }

    /* strip extra stuff from the module name */
    strip_path (mname);

    /*
     * Look up the module using the following scheme:
     *	1) look for mname as a module name
     *	2) look for mname as a directory
     *	3) look for mname as a file
     *  4) take mname up to the first slash and look it up as a module name
     *	   (this is for checking out only part of a module)
     */

    /* look it up as a module name */
    key.dptr = mname;
    key.dsize = strlen (key.dptr);
    if (db != NULL)
	val = dbm_fetch (db, key);
    else
	val.dptr = NULL;
    if (val.dptr != NULL)
    {
	/* null terminate the value  XXX - is this space ours? */
	val.dptr[val.dsize] = '\0';

	/* If the line ends in a comment, strip it off */
	if ((cp = strchr (val.dptr, '#')) != NULL)
	{
	    do
		*cp-- = '\0';
	    while (isspace (*cp));
	}
	else
	{
	    /* Always strip trailing spaces */
	    cp = strchr (val.dptr, '\0');
	    while (cp > val.dptr && isspace(*--cp))
		*cp = '\0';
	}

	value = val.dptr;
	mwhere = xstrdup (mname);
	goto found;
    }
    else
    {
	char file[PATH_MAX];
	char attic_file[PATH_MAX];
	char *acp;

	/* check to see if mname is a directory or file */

	(void) sprintf (file, "%s/%s", CVSroot_directory, mname);
	if ((acp = strrchr (mname, '/')) != NULL)
	{
	    *acp = '\0';
	    (void) sprintf (attic_file, "%s/%s/%s/%s%s", CVSroot_directory,
			    mname, CVSATTIC, acp + 1, RCSEXT);
	    *acp = '/';
	}
	else
	    (void) sprintf (attic_file, "%s/%s/%s%s", CVSroot_directory,
			    CVSATTIC, mname, RCSEXT);

	if (isdir (file))
	{
	    value = mname;
	    goto found;
	}
	else
	{
	    (void) strcat (file, RCSEXT);
	    if (isfile (file) || isfile (attic_file))
	    {
		/* if mname was a file, we have to split it into "dir file" */
		if ((cp = strrchr (mname, '/')) != NULL && cp != mname)
		{
		    char *slashp;

		    /* put the ' ' in a copy so we don't mess up the original */
		    value = strcpy (xvalue, mname);
		    slashp = strrchr (value, '/');
		    *slashp = ' ';
		}
		else
		{
		    /*
		     * the only '/' at the beginning or no '/' at all
		     * means the file we are interested in is in CVSROOT
		     * itself so the directory should be '.'
		     */
		    if (cp == mname)
		    {
			/* drop the leading / if specified */
			value = strcpy (xvalue, ". ");
			(void) strcat (xvalue, mname + 1);
		    }
		    else
		    {
			/* otherwise just copy it */
			value = strcpy (xvalue, ". ");
			(void) strcat (xvalue, mname);
		    }
		}
		goto found;
	    }
	}
    }

    /* look up everything to the first / as a module */
    if (mname[0] != '/' && (cp = strchr (mname, '/')) != NULL)
    {
	/* Make the slash the new end of the string temporarily */
	*cp = '\0';
	key.dptr = mname;
	key.dsize = strlen (key.dptr);

	/* do the lookup */
	if (db != NULL)
	    val = dbm_fetch (db, key);
	else
	    val.dptr = NULL;

	/* if we found it, clean up the value and life is good */
	if (val.dptr != NULL)
	{
	    char *cp2;

	    /* null terminate the value XXX - is this space ours? */
	    val.dptr[val.dsize] = '\0';

	    /* If the line ends in a comment, strip it off */
	    if ((cp2 = strchr (val.dptr, '#')) != NULL)
	    {
		do
		    *cp2-- = '\0';
		while (isspace (*cp2));
	    }
	    value = val.dptr;

	    /* mwhere gets just the module name */
	    mwhere = xstrdup (mname);
	    mfile = cp + 1;

	    /* put the / back in mname */
	    *cp = '/';

	    goto found;
	}

	/* put the / back in mname */
	*cp = '/';
    }

    /* if we got here, we couldn't find it using our search, so give up */
    error (0, 0, "cannot find module `%s' - ignored", mname);
    err++;
    if (mwhere)
	free (mwhere);
    return (err);


    /*
     * At this point, we found what we were looking for in one
     * of the many different forms.
     */
  found:

    /* remember where we start */
    if (save_cwd (&cwd))
	exit (EXIT_FAILURE);

    /* copy value to our own string since if we go recursive we'll be
       really screwed if we do another dbm lookup */
    zvalue = xstrdup (value);
    value = zvalue;

    /* search the value for the special delimiter and save for later */
    if ((cp = strchr (value, CVSMODULE_SPEC)) != NULL)
    {
	*cp = '\0';			/* null out the special char */
	spec_opt = cp + 1;		/* save the options for later */

	if (cp != value)		/* strip whitespace if necessary */
	    while (isspace (*--cp))
		*cp = '\0';

	if (cp == value)
	{
	    /*
	     * we had nothing but special options, so skip arg
	     * parsing and regular stuff entirely
	     *
	     * If there were only special ones though, we must
	     * make the appropriate directory and cd to it
	     */
	    char *dir;

	    /* XXX - XXX - MAJOR HACK - DO NOT SHIP - this needs to
	       be !pipeout, but we don't know that here yet */
	    if (!run_module_prog)
		goto out;

	    dir = where ? where : mname;
	    /* XXX - think about making null repositories at each dir here
		     instead of just at the bottom */
	    make_directories (dir);
	    if ( CVS_CHDIR (dir) < 0)
	    {
		error (0, errno, "cannot chdir to %s", dir);
		spec_opt = NULL;
		err++;
		goto out;
	    }
	    if (!isfile (CVSADM))
	    {
		char nullrepos[PATH_MAX];

		(void) sprintf (nullrepos, "%s/%s/%s", CVSroot_directory,
				CVSROOTADM, CVSNULLREPOS);
		if (!isfile (nullrepos))
		{
		    mode_t omask;
		    omask = umask (cvsumask);
		    (void) CVS_MKDIR (nullrepos, 0777);
		    (void) umask (omask);
		}
		if (!isdir (nullrepos))
		    error (1, 0, "there is no repository %s", nullrepos);

		Create_Admin (".", dir,
			      nullrepos, (char *) NULL, (char *) NULL);
		if (!noexec)
		{
		    FILE *fp;

		    fp = open_file (CVSADM_ENTSTAT, "w+");
		    if (fclose (fp) == EOF)
			error (1, errno, "cannot close %s", CVSADM_ENTSTAT);
#ifdef SERVER_SUPPORT
		    if (server_active)
			server_set_entstat (dir, nullrepos);
#endif
		}
	    }
	  out:
	    goto do_special;
	}
    }

    /* don't do special options only part of a module was specified */
    if (mfile != NULL)
	spec_opt = NULL;

    /*
     * value now contains one of the following:
     *    1) dir
     *	  2) dir file
     *    3) the value from modules without any special args
     *		    [ args ] dir [file] [file] ...
     *	     or     -a module [ module ] ...
     */

    /* Put the value on a line with XXX prepended for getopt to eat */
    line = xmalloc (strlen (value) + 10);
    (void) sprintf (line, "%s %s", "XXX", value);

    /* turn the line into an argv[] array */
    line2argv (&xmodargc, xmodargv, line);
    free (line);
    modargc = xmodargc;
    modargv = xmodargv;

    /* parse the args */
    optind = 1;
    while ((c = getopt (modargc, modargv, CVSMODULE_OPTS)) != -1)
    {
	switch (c)
	{
	    case 'a':
		alias = 1;
		break;
	    case 'd':
		if (mwhere)
		    free (mwhere);
		mwhere = xstrdup (optarg);
		break;
	    case 'i':
		checkin_prog = optarg;
		break;
	    case 'l':
		local_specified = 1;
		break;
	    case 'o':
		checkout_prog = optarg;
		break;
	    case 'e':
		export_prog = optarg;
		break;
	    case 't':
		tag_prog = optarg;
		break;
	    case 'u':
		update_prog = optarg;
		break;
	    case '?':
		error (0, 0,
		       "modules file has invalid option for key %s value %s",
		       key.dptr, val.dptr);
		err++;
		if (mwhere)
		    free (mwhere);
		free (zvalue);
		free_cwd (&cwd);
		return (err);
	}
    }
    modargc -= optind;
    modargv += optind;
    if (modargc == 0)
    {
	error (0, 0, "modules file missing directory for module %s", mname);
	if (mwhere)
	    free (mwhere);
	free (zvalue);
	free_cwd (&cwd);
	return (++err);
    }

    /* if this was an alias, call ourselves recursively for each module */
    if (alias)
    {
	int i;

	for (i = 0; i < modargc; i++)
	{
	    if (strcmp (mname, modargv[i]) == 0)
		error (0, 0,
		       "module `%s' in modules file contains infinite loop",
		       mname);
	    else
		err += do_module (db, modargv[i], m_type, msg, callback_proc,
				  where, shorten, local_specified,
				  run_module_prog, extra_arg);
	}
	if (mwhere)
	    free (mwhere);
	free (zvalue);
	free_cwd (&cwd);
	return (err);
    }

    /* otherwise, process this module */
    err += callback_proc (&modargc, modargv, where, mwhere, mfile, shorten,
			  local_specified, mname, msg);

#if 0
    /* FIXME: I've fixed this so that the correct arguments are called, 
       but now this fails because there is code below this point that
       uses optarg values extracted from the arg vector. */
    free_names (&xmodargc, xmodargv);
#endif

    /* if there were special include args, process them now */

  do_special:

    /* blow off special options if -l was specified */
    if (local_specified)
	spec_opt = NULL;

    while (spec_opt != NULL)
    {
	char *next_opt;

	cp = strchr (spec_opt, CVSMODULE_SPEC);
	if (cp != NULL)
	{
	    /* save the beginning of the next arg */
	    next_opt = cp + 1;

	    /* strip whitespace off the end */
	    do
		*cp = '\0';
	    while (isspace (*--cp));
	}
	else
	    next_opt = NULL;

	/* strip whitespace from front */
	while (isspace (*spec_opt))
	    spec_opt++;

	if (*spec_opt == '\0')
	    error (0, 0, "Mal-formed %c option for module %s - ignored",
		   CVSMODULE_SPEC, mname);
	else
	    err += do_module (db, spec_opt, m_type, msg, callback_proc,
			      (char *) NULL, 0, local_specified,
			      run_module_prog, extra_arg);
	spec_opt = next_opt;
    }

    /* write out the checkin/update prog files if necessary */
#ifdef SERVER_SUPPORT
    if (err == 0 && !noexec && m_type == CHECKOUT && server_expanding)
    {
	if (checkin_prog != NULL)
	    server_prog (where ? where : mname, checkin_prog, PROG_CHECKIN);
	if (update_prog != NULL)
	    server_prog (where ? where : mname, update_prog, PROG_UPDATE);
    }
    else
#endif
    if (err == 0 && !noexec && m_type == CHECKOUT && run_module_prog)
    {
	FILE *fp;

	if (checkin_prog != NULL)
	{
	    fp = open_file (CVSADM_CIPROG, "w+");
	    (void) fprintf (fp, "%s\n", checkin_prog);
	    if (fclose (fp) == EOF)
		error (1, errno, "cannot close %s", CVSADM_CIPROG);
	}
	if (update_prog != NULL)
	{
	    fp = open_file (CVSADM_UPROG, "w+");
	    (void) fprintf (fp, "%s\n", update_prog);
	    if (fclose (fp) == EOF)
		error (1, errno, "cannot close %s", CVSADM_UPROG);
	}
    }

    /* cd back to where we started */
    if (restore_cwd (&cwd, NULL))
	exit (EXIT_FAILURE);
    free_cwd (&cwd);

    /* run checkout or tag prog if appropriate */
    if (err == 0 && run_module_prog)
    {
	if ((m_type == TAG && tag_prog != NULL) ||
	    (m_type == CHECKOUT && checkout_prog != NULL) ||
	    (m_type == EXPORT && export_prog != NULL))
	{
	    /*
	     * If a relative pathname is specified as the checkout, tag
	     * or export proc, try to tack on the current "where" value.
	     * if we can't find a matching program, just punt and use
	     * whatever is specified in the modules file.
	     */
	    char real_prog[PATH_MAX];
	    char *prog = (m_type == TAG ? tag_prog :
			  (m_type == CHECKOUT ? checkout_prog : export_prog));
	    char *real_where = (where != NULL ? where : mwhere);
	    char *expanded_path;

	    if ((*prog != '/') && (*prog != '.'))
	    {
		(void) sprintf (real_prog, "%s/%s", real_where, prog);
		if (isfile (real_prog))
		    prog = real_prog;
	    }

	    /* XXX can we determine the line number for this entry??? */
	    expanded_path = expand_path (prog, "modules", 0);
	    if (expanded_path != NULL)
	    {
		run_setup ("%s %s", expanded_path, real_where);

		if (extra_arg)
		    run_arg (extra_arg);

		if (!quiet)
		{
		    (void) printf ("%s %s: Executing '", program_name,
				   command_name);
		    run_print (stdout);
		    (void) printf ("'\n");
		}
		err += run_exec (RUN_TTY, RUN_TTY, RUN_TTY, RUN_NORMAL);
		free (expanded_path);
	    }
	}
    }

    /* clean up */
    if (mwhere)
	free (mwhere);
    free (zvalue);

    return (err);
}

/* - Read all the records from the modules database into an array.
   - Sort the array depending on what format is desired.
   - Print the array in the format desired.

   Currently, there are only two "desires":

   1. Sort by module name and format the whole entry including switches,
      files and the comment field: (Including aliases)

      modulename	-s switches, one per line, even if
			-i it has many switches.
			Directories and files involved, formatted
			to cover multiple lines if necessary.
			# Comment, also formatted to cover multiple
			# lines if necessary.

   2. Sort by status field string and print:  (*not* including aliases)

      modulename    STATUS	Directories and files involved, formatted
				to cover multiple lines if necessary.
				# Comment, also formatted to cover multiple
				# lines if necessary.
*/

static struct sortrec *s_head;

static int s_max = 0;			/* Number of elements allocated */
static int s_count = 0;			/* Number of elements used */

static int Status;		        /* Nonzero if the user is
					   interested in status
					   information as well as
					   module name */
static char def_status[] = "NONE";

/* Sort routine for qsort:
   - If we want the "Status" field to be sorted, check it first.
   - Then compare the "module name" fields.  Since they are unique, we don't
     have to look further.
*/
static int
sort_order (l, r)
    const PTR l;
    const PTR r;
{
    int i;
    const struct sortrec *left = (const struct sortrec *) l;
    const struct sortrec *right = (const struct sortrec *) r;

    if (Status)
    {
	/* If Sort by status field, compare them. */
	if ((i = strcmp (left->status, right->status)) != 0)
	    return (i);
    }
    return (strcmp (left->modname, right->modname));
}

static void
save_d (k, ks, d, ds)
    char *k;
    int ks;
    char *d;
    int ds;
{
    char *cp, *cp2;
    struct sortrec *s_rec;

    if (Status && *d == '-' && *(d + 1) == 'a')
	return;				/* We want "cvs co -s" and it is an alias! */

    if (s_count == s_max)
    {
	s_max += 64;
	s_head = (struct sortrec *) xrealloc ((char *) s_head, s_max * sizeof (*s_head));
    }
    s_rec = &s_head[s_count];
    s_rec->modname = cp = xmalloc (ks + 1);
    (void) strncpy (cp, k, ks);
    *(cp + ks) = '\0';

    s_rec->rest = cp2 = xmalloc (ds + 1);
    cp = d;
    *(cp + ds) = '\0';	/* Assumes an extra byte at end of static dbm buffer */

    while (isspace (*cp))
	cp++;
    /* Turn <spaces> into one ' ' -- makes the rest of this routine simpler */
    while (*cp)
    {
	if (isspace (*cp))
	{
	    *cp2++ = ' ';
	    while (isspace (*cp))
		cp++;
	}
	else
	    *cp2++ = *cp++;
    }
    *cp2 = '\0';

    /* Look for the "-s statusvalue" text */
    if (Status)
    {
	s_rec->status = def_status;

	/* Minor kluge, but general enough to maintain */
	for (cp = s_rec->rest; (cp2 = strchr (cp, '-')) != NULL; cp = ++cp2)
	{
	    if (*(cp2 + 1) == 's' && *(cp2 + 2) == ' ')
	    {
		s_rec->status = (cp2 += 3);
		while (*cp2 != ' ')
		    cp2++;
		*cp2++ = '\0';
		cp = cp2;
		break;
	    }
	}
    }
    else
	cp = s_rec->rest;

    /* Find comment field, clean up on all three sides & compress blanks */
    if ((cp2 = cp = strchr (cp, '#')) != NULL)
    {
	if (*--cp2 == ' ')
	    *cp2 = '\0';
	if (*++cp == ' ')
	    cp++;
	s_rec->comment = cp;
    }
    else
	s_rec->comment = "";

    s_count++;
}

/* Print out the module database as we know it.  If STATUS is
   non-zero, print out status information for each module. */

void
cat_module (status)
    int status;
{
    DBM *db;
    datum key, val;
    int i, c, wid, argc, cols = 80, indent, fill;
    int moduleargc;
    struct sortrec *s_h;
    char *cp, *cp2, **argv;
    char *line;
    char *moduleargv[MAXFILEPERDIR];

    Status = status;

    /* Read the whole modules file into allocated records */
    if (!(db = open_module ()))
	error (1, 0, "failed to open the modules file");

    for (key = dbm_firstkey (db); key.dptr != NULL; key = dbm_nextkey (db))
    {
	val = dbm_fetch (db, key);
	if (val.dptr != NULL)
	    save_d (key.dptr, key.dsize, val.dptr, val.dsize);
    }

    /* Sort the list as requested */
    qsort ((PTR) s_head, s_count, sizeof (struct sortrec), sort_order);

    /*
     * Run through the sorted array and format the entries
     * indent = space for modulename + space for status field
     */
    indent = 12 + (status * 12);
    fill = cols - (indent + 2);
    for (s_h = s_head, i = 0; i < s_count; i++, s_h++)
    {
	/* Print module name (and status, if wanted) */
	(void) printf ("%-12s", s_h->modname);
	if (status)
	{
	    (void) printf (" %-11s", s_h->status);
	    if (s_h->status != def_status)
		*(s_h->status + strlen (s_h->status)) = ' ';
	}

	/* Parse module file entry as command line and print options */
	line = xmalloc (strlen (s_h->modname) + strlen (s_h->rest) + 10);
	(void) sprintf (line, "%s %s", s_h->modname, s_h->rest);
	line2argv (&moduleargc, moduleargv, line);
	free (line);
	argc = moduleargc;
	argv = moduleargv;

	optind = 0;
	wid = 0;
	while ((c = getopt (argc, argv, CVSMODULE_OPTS)) != -1)
	{
	    if (!status)
	    {
		if (c == 'a' || c == 'l')
		{
		    (void) printf (" -%c", c);
		    wid += 3;		/* Could just set it to 3 */
		}
		else
		{
		    if (strlen (optarg) + 4 + wid > (unsigned) fill)
		    {
			(void) printf ("\n%*s", indent, "");
			wid = 0;
		    }
		    (void) printf (" -%c %s", c, optarg);
		    wid += strlen (optarg) + 4;
		}
	    }
	}
	argc -= optind;
	argv += optind;

	/* Format and Print all the files and directories */
	for (; argc--; argv++)
	{
	    if (strlen (*argv) + wid > (unsigned) fill)
	    {
		(void) printf ("\n%*s", indent, "");
		wid = 0;
	    }
	    (void) printf (" %s", *argv);
	    wid += strlen (*argv) + 1;
	}
	(void) printf ("\n");

	/* Format the comment field -- save_d (), compressed spaces */
	for (cp2 = cp = s_h->comment; *cp; cp2 = cp)
	{
	    (void) printf ("%*s # ", indent, "");
	    if (strlen (cp2) < (unsigned) (fill - 2))
	    {
		(void) printf ("%s\n", cp2);
		break;
	    }
	    cp += fill - 2;
	    while (*cp != ' ' && cp > cp2)
		cp--;
	    if (cp == cp2)
	    {
		(void) printf ("%s\n", cp2);
		break;
	    }

	    *cp++ = '\0';
	    (void) printf ("%s\n", cp2);
	}

	free_names(&moduleargc, moduleargv);
    }
}
