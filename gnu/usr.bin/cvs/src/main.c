/*
 *    Copyright (c) 1992, Brian Berliner and Jeff Polk
 *    Copyright (c) 1989-1992, Brian Berliner
 *
 *    You may distribute under the terms of the GNU General Public License
 *    as specified in the README file that comes with the CVS source distribution.
 *
 * This is the main C driver for the CVS system.
 *
 * Credit to Dick Grune, Vrije Universiteit, Amsterdam, for writing
 * the shell-script CVS system that this is based on.
 *
 */

#include "cvs.h"

#ifdef HAVE_WINSOCK_H
#include <winsock.h>
#else
extern int gethostname ();
#endif

char *program_name;
char *program_path;
char *command_name;

/* I'd dynamically allocate this, but it seems like gethostname
   requires a fixed size array.  If I'm remembering the RFCs right,
   256 should be enough.  */
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN  256
#endif

char hostname[MAXHOSTNAMELEN];

int use_editor = 1;
int use_cvsrc = 1;
int cvswrite = !CVSREAD_DFLT;
int really_quiet = 0;
int quiet = 0;
int trace = 0;
int noexec = 0;
int logoff = 0;

/* Set if we should be writing CVSADM directories at top level.  At
   least for now we'll make the default be off (the CVS 1.9, not CVS
   1.9.2, behavior). */
int top_level_admin = 0;

mode_t cvsumask = UMASK_DFLT;

char *CurDir;

/*
 * Defaults, for the environment variables that are not set
 */
char *Tmpdir = TMPDIR_DFLT;
char *Editor = EDITOR_DFLT;

static const struct cmd
{
    char *fullname;		/* Full name of the function (e.g. "commit") */

    /* Synonyms for the command, nick1 and nick2.  We supply them
       mostly for two reasons: (1) CVS has always supported them, and
       we need to maintain compatibility, (2) if there is a need for a
       version which is shorter than the fullname, for ease in typing.
       Synonyms have the disadvantage that people will see "new" and
       then have to think about it, or look it up, to realize that is
       the operation they know as "add".  Also, this means that one
       cannot create a command "cvs new" with a different meaning.  So
       new synonyms are probably best used sparingly, and where used
       should be abbreviations of the fullname (preferably consisting
       of the first 2 or 3 or so letters).

       One thing that some systems do is to recognize any unique
       abbreviation, for example "annotat" "annota", etc., for
       "annotate".  The problem with this is that scripts and user
       habits will expect a certain abbreviation to be unique, and in
       a future release of CVS it may not be.  So it is better to
       accept only an explicit list of abbreviations and plan on
       supporting them in the future as well as now.  */

    char *nick1;
    char *nick2;
    
    int (*func) ();		/* Function takes (argc, argv) arguments. */
} cmds[] =

{
    { "add",      "ad",       "new",       add },
    { "admin",    "adm",      "rcs",       admin },
    { "annotate", "ann",      NULL,        annotate },
    { "checkout", "co",       "get",       checkout },
    { "commit",   "ci",       "com",       commit },
    { "diff",     "di",       "dif",       diff },
    { "edit",     NULL,	      NULL,	   edit },
    { "editors",  NULL,       NULL,	   editors },
    { "export",   "exp",      "ex",        checkout },
    { "history",  "hi",       "his",       history },
    { "import",   "im",       "imp",       import },
    { "init",     NULL,       NULL,        init },
#ifdef SERVER_SUPPORT
    { "kserver",  NULL,       NULL,        server }, /* placeholder */
#endif
    { "log",      "lo",       "rlog",      cvslog },
#ifdef AUTH_CLIENT_SUPPORT
    { "login",    "logon",    "lgn",       login },
    { "logout",   NULL,       NULL,        logout },
#ifdef SERVER_SUPPORT
    { "pserver",  NULL,       NULL,        server }, /* placeholder */
#endif
#endif /* AUTH_CLIENT_SUPPORT */
    { "rdiff",    "patch",    "pa",        patch },
    { "release",  "re",       "rel",       release },
    { "remove",   "rm",       "delete",    cvsremove },
    { "status",   "st",       "stat",      cvsstatus },
    { "rtag",     "rt",       "rfreeze",   rtag },
    { "tag",      "ta",       "freeze",    cvstag },
    { "unedit",   NULL,	      NULL,	   unedit },
    { "update",   "up",       "upd",       update },
    { "watch",    NULL,	      NULL,	   watch },
    { "watchers", NULL,	      NULL,	   watchers },
#ifdef SERVER_SUPPORT
    { "server",   NULL,       NULL,        server },
#endif
    { NULL, NULL, NULL, NULL },
};

static const char *const usg[] =
{
    /* CVS usage messages never have followed the GNU convention of
       putting metavariables in uppercase.  I don't know whether that
       is a good convention or not, but if it changes it would have to
       change in all the usage messages.  For now, they consistently
       use lowercase, as far as I know.  Puncutation is pretty funky,
       though.  Sometimes they use none, as here.  Sometimes they use
       single quotes (not the TeX-ish `' stuff), as in --help-options.
       Sometimes they use double quotes, as in cvs -H add.

       Most (not all) of the usage messages seem to have periods at
       the end of each line.  I haven't tried to duplicate this style
       in --help as it is a rather different format from the rest.  */

    "Usage: %s [cvs-options] command [command-options-and-arguments]\n",
    "  where cvs-options are -q, -n, etc.\n",
    "    (specify --help-options for a list of options)\n",
    "  where command is add, admin, etc.\n",
    "    (specify --help-commands for a list of commands\n",
    "     or --help-synonyms for a list of command synonyms)\n",
    "  where command-options-and-arguments depend on the specific command\n",
    "    (specify -H followed by a command name for command-specific help)\n",
    "  Specify --help to receive this message\n",
    "\n",

    /* Some people think that a bug-reporting address should go here.  IMHO,
       the web sites are better because anything else is very likely to go
       obsolete in the years between a release and when someone might be
       reading this help.  Besides, we could never adequately discuss
       bug reporting in a concise enough way to put in a help message.  */

    /* I was going to put this at the top, but usage() wants the %s to
       be in the first line.  */
    "The Concurrent Versions System (CVS) is a tool for version control.\n",
    /* I really don't think I want to try to define "version control"
       in one line.  I'm not sure one can get more concise than the
       paragraph in ../cvs.spec without assuming the reader knows what
       version control means.  */

    "For CVS updates and additional information, see\n",
    "    Cyclic Software at http://www.cyclic.com/ or\n",
    "    Pascal Molli's CVS site at http://www.loria.fr/~molli/cvs-index.html\n",
    NULL,
};

static const char *const cmd_usage[] =
{
    "CVS commands are:\n",
    "        add          Add a new file/directory to the repository\n",
    "        admin        Administration front end for rcs\n",
    "        annotate     Show last revision where each line was modified\n",
    "        checkout     Checkout sources for editing\n",
    "        commit       Check files into the repository\n",
    "        diff         Show differences between revisions\n",
    "        edit         Get ready to edit a watched file\n",
    "        editors      See who is editing a watched file\n",
    "        export       Export sources from CVS, similar to checkout\n",
    "        history      Show repository access history\n",
    "        import       Import sources into CVS, using vendor branches\n",
    "        init         Create a CVS repository if it doesn't exist\n",
    "        log          Print out history information for files\n",
#ifdef AUTH_CLIENT_SUPPORT
    "        login        Prompt for password for authenticating server.\n",
    "        logout       Removes entry in .cvspass for remote repository.\n",
#endif /* AUTH_CLIENT_SUPPORT */
    "        rdiff        Create 'patch' format diffs between releases\n",
    "        release      Indicate that a Module is no longer in use\n",
    "        remove       Remove an entry from the repository\n",
    "        rtag         Add a symbolic tag to a module\n",
    "        status       Display status information on checked out files\n",
    "        tag          Add a symbolic tag to checked out version of files\n",
    "        unedit       Undo an edit command\n",
    "        update       Bring work tree in sync with repository\n",
    "        watch        Set watches\n",
    "        watchers     See who is watching a file\n",
    "(Specify the --help option for a list of other help options)\n",
    NULL,
};

static const char *const opt_usage[] =
{
    "CVS global options (specified before the command name) are:\n",
    "    -H           Displays usage information for command.\n",
    "    -Q           Cause CVS to be really quiet.\n",
    "    -q           Cause CVS to be somewhat quiet.\n",
    "    -r           Make checked-out files read-only.\n",
    "    -w           Make checked-out files read-write (default).\n",
    "    -l           Turn history logging off.\n",
    "    -n           Do not execute anything that will change the disk.\n",
    "    -t           Show trace of program execution -- try with -n.\n",
    "    -v           CVS version and copyright.\n",
    "    -b bindir    Find RCS programs in 'bindir'.\n",
    "    -T tmpdir    Use 'tmpdir' for temporary files.\n",
    "    -e editor    Use 'editor' for editing log information.\n",
    "    -d CVS_root  Overrides $CVSROOT as the root of the CVS tree.\n",
    "    -f           Do not use the ~/.cvsrc file.\n",
#ifdef CLIENT_SUPPORT
    "    -z #         Use compression level '#' for net traffic.\n",
#ifdef ENCRYPTION
    "    -x           Encrypt all net traffic.\n",
#endif
    "    -a           Authenticate all net traffic.\n",
#endif
    "    -s VAR=VAL   Set CVS user variable.\n",
    "(Specify the --help option for a list of other help options)\n",
    NULL
};

static const char * const*
cmd_synonyms ()
{
    char ** synonyms;
    char ** line;
    const struct cmd *c = &cmds[0];
    /* Three more for title, "specify --help" line, and NULL.  */
    int numcmds = 3;

    while (c->fullname != NULL)
    {
	numcmds++;
	c++;
    }
    
    synonyms = (char **) xmalloc(numcmds * sizeof(char *));
    line = synonyms;
    *line++ = "CVS command synonyms are:\n";
    for (c = &cmds[0]; c->fullname != NULL; c++)
    {
	if (c->nick1 || c->nick2)
	{
	    *line = xmalloc (strlen (c->fullname)
			     + (c->nick1 != NULL ? strlen (c->nick1) : 0)
			     + (c->nick2 != NULL ? strlen (c->nick2) : 0)
			     + 40);
	    sprintf(*line, "        %-12s %s %s\n", c->fullname,
		    c->nick1 ? c->nick1 : "",
		    c->nick2 ? c->nick2 : "");
	    line++;
	}
    }
    *line++ = "(Specify the --help option for a list of other help options)\n";
    *line = NULL;
    
    return (const char * const*) synonyms; /* will never be freed */
}


unsigned long int
lookup_command_attribute (cmd_name)
     char *cmd_name;
{
    unsigned long int ret = 0;

    if (strcmp (cmd_name, "import") != 0)
    {
        ret |= CVS_CMD_IGNORE_ADMROOT;
    }


    /* The following commands do not use a checked-out working
       directory.  We conservatively assume that everything else does.
       Feel free to add to this list if you are _certain_ something
       something doesn't use the WD. */
    if ((strcmp (cmd_name, "checkout") != 0) &&
        (strcmp (cmd_name, "init") != 0) &&
        (strcmp (cmd_name, "login") != 0) &&
	(strcmp (cmd_name, "logout") != 0) &&
        (strcmp (cmd_name, "rdiff") != 0) &&
        (strcmp (cmd_name, "release") != 0) &&
        (strcmp (cmd_name, "rtag") != 0))
    {
        ret |= CVS_CMD_USES_WORK_DIR;
    }


    /* The following commands do not modify the repository; we
       conservatively assume that everything else does.  Feel free to
       add to this list if you are _certain_ something is safe. */
    if ((strcmp (cmd_name, "annotate") != 0) &&
        (strcmp (cmd_name, "checkout") != 0) &&
        (strcmp (cmd_name, "diff") != 0) &&
        (strcmp (cmd_name, "rdiff") != 0) &&
        (strcmp (cmd_name, "update") != 0) &&
        (strcmp (cmd_name, "history") != 0) &&
        (strcmp (cmd_name, "editors") != 0) &&
        (strcmp (cmd_name, "export") != 0) &&
        (strcmp (cmd_name, "history") != 0) &&
        (strcmp (cmd_name, "log") != 0) &&
        (strcmp (cmd_name, "noop") != 0) &&
        (strcmp (cmd_name, "watchers") != 0) &&
        (strcmp (cmd_name, "status") != 0))
    {
        ret |= CVS_CMD_MODIFIES_REPOSITORY;
    }

    return ret;
}


static RETSIGTYPE
main_cleanup (sig)
    int sig;
{
#ifndef DONT_USE_SIGNALS
    const char *name;
    char temp[10];

    switch (sig)
    {
#ifdef SIGHUP
    case SIGHUP:
	name = "hangup";
	break;
#endif
#ifdef SIGINT
    case SIGINT:
	name = "interrupt";
	break;
#endif
#ifdef SIGQUIT
    case SIGQUIT:
	name = "quit";
	break;
#endif
#ifdef SIGPIPE
    case SIGPIPE:
	name = "broken pipe";
	break;
#endif
#ifdef SIGTERM
    case SIGTERM:
	name = "termination";
	break;
#endif
    default:
	/* This case should never be reached, because we list above all
	   the signals for which we actually establish a signal handler.  */
	sprintf (temp, "%d", sig);
	name = temp;
	break;
    }

    error (1, 0, "received %s signal", name);
#endif /* !DONT_USE_SIGNALS */
}

int
main (argc, argv)
    int argc;
    char **argv;
{
    char *CVSroot = CVSROOT_DFLT;
    extern char *version_string;
    extern char *config_string;
    char *cp, *end;
    const struct cmd *cm;
    int c, err = 0;
    int tmpdir_update_env, cvs_update_env;
    int free_CVSroot = 0;
    int free_Editor = 0;
    int free_Tmpdir = 0;

    int help = 0;		/* Has the user asked for help?  This
				   lets us support the `cvs -H cmd'
				   convention to give help for cmd. */
    static struct option long_options[] =
    {
        {"help", 0, NULL, 'H'},
        {"version", 0, NULL, 'v'},
	{"help-commands", 0, NULL, 1},
	{"help-synonyms", 0, NULL, 2},
	{"help-options", 0, NULL, 4},
	{"allow-root", required_argument, NULL, 3},
        {0, 0, 0, 0}
    };
    /* `getopt_long' stores the option index here, but right now we
        don't use it. */
    int option_index = 0;
    int need_to_create_root = 0;

#ifdef SYSTEM_INITIALIZE
    /* Hook for OS-specific behavior, for example socket subsystems on
       NT and OS2 or dealing with windows and arguments on Mac.  */
    SYSTEM_INITIALIZE (&argc, &argv);
#endif

#ifdef HAVE_TZSET
    /* On systems that have tzset (which is almost all the ones I know
       of), it's a good idea to call it.  */
    tzset ();
#endif

    /*
     * Just save the last component of the path for error messages
     */
    program_path = xstrdup (argv[0]);
#ifdef ARGV0_NOT_PROGRAM_NAME
    /* On some systems, e.g. VMS, argv[0] is not the name of the command
       which the user types to invoke the program.  */
    program_name = "cvs";
#else
    program_name = last_component (argv[0]);
#endif

    /*
     * Query the environment variables up-front, so that
     * they can be overridden by command line arguments
     */
    cvs_update_env = 0;
    tmpdir_update_env = *Tmpdir;	/* TMPDIR_DFLT must be set */
    if ((cp = getenv (TMPDIR_ENV)) != NULL)
    {
	Tmpdir = cp;
	tmpdir_update_env = 0;		/* it's already there */
    }
    if ((cp = getenv (EDITOR1_ENV)) != NULL)
 	Editor = cp;
    else if ((cp = getenv (EDITOR2_ENV)) != NULL)
	Editor = cp;
    else if ((cp = getenv (EDITOR3_ENV)) != NULL)
	Editor = cp;
    if ((cp = getenv (CVSROOT_ENV)) != NULL)
    {
	CVSroot = cp;
	cvs_update_env = 0;		/* it's already there */
    }
    if (getenv (CVSREAD_ENV) != NULL)
	cvswrite = 0;

    /* Set this to 0 to force getopt initialization.  getopt() sets
       this to 1 internally.  */
    optind = 0;

    /* We have to parse the options twice because else there is no
       chance to avoid reading the global options from ".cvsrc".  Set
       opterr to 0 for avoiding error messages about invalid options.
       */
    opterr = 0;

    while ((c = getopt_long
            (argc, argv, "+f", NULL, NULL))
           != EOF)
    {
	if (c == 'f')
	    use_cvsrc = 0;
    }

    /*
     * Scan cvsrc file for global options.
     */
    if (use_cvsrc)
	read_cvsrc (&argc, &argv, "cvs");

    optind = 0;
    opterr = 1;

    while ((c = getopt_long
            (argc, argv, "+Qqrwtnlvb:T:e:d:Hfz:s:xa", long_options, &option_index))
           != EOF)
    {
	switch (c)
	{
            case 1:
	        /* --help-commands */
                usage (cmd_usage);
                break;
            case 2:
	        /* --help-synonyms */
                usage (cmd_synonyms());
                break;
	    case 4:
		/* --help-options */
		usage (opt_usage);
		break;
	    case 3:
		/* --allow-root */
		root_allow_add (optarg);
		break;
	    case 'Q':
		really_quiet = 1;
		/* FALL THROUGH */
	    case 'q':
		quiet = 1;
		break;
	    case 'r':
		cvswrite = 0;
		break;
	    case 'w':
		cvswrite = 1;
		break;
	    case 't':
		trace = 1;
		break;
	    case 'n':
		noexec = 1;
	    case 'l':			/* Fall through */
		logoff = 1;
		break;
	    case 'v':
		/* Having the year here is a good idea, so people have
		   some idea of how long ago their version of CVS was
		   released.  */
		(void) fputs (version_string, stdout);
		(void) fputs (config_string, stdout);
		(void) fputs ("\n", stdout);
		(void) fputs ("\
Copyright (c) 1989-1998 Brian Berliner, david d `zoo' zuhn, \n\
                        Jeff Polk, and other authors\n", stdout);
		(void) fputs ("\n", stdout);
		(void) fputs ("CVS may be copied only under the terms of the GNU General Public License,\n", stdout);
		(void) fputs ("a copy of which can be found with the CVS distribution kit.\n", stdout);
		(void) fputs ("\n", stdout);

		(void) fputs ("Specify the --help option for further information about CVS\n", stdout);

		exit (0);
		break;
	    case 'b':
		/* This option used to specify the directory for RCS
		   executables.  But since we don't run them any more,
		   this is a noop.  Silently ignore it so that .cvsrc
		   and scripts and inetd.conf and such can work with
		   either new or old CVS.  */
		break;
	    case 'T':
		Tmpdir = xstrdup (optarg);
		free_Tmpdir = 1;
		tmpdir_update_env = 1;	/* need to update environment */
		break;
	    case 'e':
		Editor = xstrdup (optarg);
		free_Editor = 1;
		break;
	    case 'd':
		CVSroot = xstrdup (optarg);
		free_CVSroot = 1;
		cvs_update_env = 1;	/* need to update environment */
		break;
	    case 'H':
	        help = 1;
		break;
            case 'f':
		use_cvsrc = 0; /* unnecessary, since we've done it above */
		break;
	    case 'z':
#ifdef CLIENT_SUPPORT
		gzip_level = atoi (optarg);
		if (gzip_level <= 0 || gzip_level > 9)
		  error (1, 0,
			 "gzip compression level must be between 1 and 9");
#endif
		/* If no CLIENT_SUPPORT, we just silently ignore the gzip
		   level, so that users can have it in their .cvsrc and not
		   cause any trouble.  */
		break;
	    case 's':
		variable_set (optarg);
		break;
	    case 'x':
#ifdef CLIENT_SUPPORT
	        cvsencrypt = 1;
#endif /* CLIENT_SUPPORT */
		/* If no CLIENT_SUPPORT, ignore -x, so that users can
                   have it in their .cvsrc and not cause any trouble.
                   If no ENCRYPTION, we still accept -x, but issue an
                   error if we are being run as a client.  */
		break;
	    case 'a':
#ifdef CLIENT_SUPPORT
		cvsauthenticate = 1;
#endif
		/* If no CLIENT_SUPPORT, ignore -a, so that users can
                   have it in their .cvsrc and not cause any trouble.
                   We will issue an error later if stream
                   authentication is not supported.  */
		break;
	    case '?':
	    default:
                usage (usg);
	}
    }

    argc -= optind;
    argv += optind;
    if (argc < 1)
	usage (usg);


    /* Look up the command name. */

    command_name = argv[0];
    for (cm = cmds; cm->fullname; cm++)
    {
	if (cm->nick1 && !strcmp (command_name, cm->nick1))
	    break;
	if (cm->nick2 && !strcmp (command_name, cm->nick2))
	    break;
	if (!strcmp (command_name, cm->fullname))
	    break;
    }

    if (!cm->fullname)
	usage (cmd_usage);	        /* no match */
    else
	command_name = cm->fullname;	/* Global pointer for later use */

    /* This should probably remain a warning, rather than an error,
       for quite a while.  For one thing the version of VC distributed
       with GNU emacs 19.34 invokes 'cvs rlog' instead of 'cvs log'.  */
    if (strcmp (argv[0], "rlog") == 0)
    {
	error (0, 0, "warning: the rlog command is deprecated");
	error (0, 0, "use the synonymous log command instead");
    }

    if (help)
	argc = -1;		/* some functions only check for this */
    else
    {
	/* The user didn't ask for help, so go ahead and authenticate,
           set up CVSROOT, and the rest of it. */

	/* The UMASK environment variable isn't handled with the
	   others above, since we don't want to signal errors if the
	   user has asked for help.  This won't work if somebody adds
	   a command-line flag to set the umask, since we'll have to
	   parse it before we get here. */

	if ((cp = getenv (CVSUMASK_ENV)) != NULL)
	{
	    /* FIXME: Should be accepting symbolic as well as numeric mask.  */
	    cvsumask = strtol (cp, &end, 8) & 0777;
	    if (*end != '\0')
		error (1, errno, "invalid umask value in %s (%s)",
		       CVSUMASK_ENV, cp);
	}

#if defined (HAVE_KERBEROS) && defined (SERVER_SUPPORT)
	/* If we are invoked with a single argument "kserver", then we are
	   running as Kerberos server as root.  Do the authentication as
	   the very first thing, to minimize the amount of time we are
	   running as root.  */
	if (strcmp (command_name, "kserver") == 0)
	{
	    kserver_authenticate_connection ();

	    /* Pretend we were invoked as a plain server.  */
	    command_name = "server";
	}
#endif /* HAVE_KERBEROS */


#if (defined(AUTH_SERVER_SUPPORT) || defined (HAVE_GSSAPI)) && defined(SERVER_SUPPORT)
	if (strcmp (command_name, "pserver") == 0)
	{
	    /* The reason that --allow-root is not a command option
	       is mainly the comment in server() about how argc,argv
	       might be from .cvsrc.  I'm not sure about that, and
	       I'm not sure it is only true of command options, but
	       it seems easier to make it a global option.  */

	    /* Gets username and password from client, authenticates, then
	       switches to run as that user and sends an ACK back to the
	       client. */
	    pserver_authenticate_connection ();
      
	    /* Pretend we were invoked as a plain server.  */
	    command_name = "server";
	}
#endif /* (AUTH_SERVER_SUPPORT || HAVE_GSSAPI) && SERVER_SUPPORT */

#ifdef SERVER_SUPPORT
	server_active = strcmp (command_name, "server") == 0;

	/* Fiddling with CVSROOT doesn't make sense if we're running
           in server mode, since the client will send the repository
           directory after the connection is made. */

	if (!server_active)
#endif
	{
	    char *CVSADM_Root;
	    
	    /* See if we are able to find a 'better' value for CVSroot
	       in the CVSADM_ROOT directory. */

	    CVSADM_Root = NULL;

	    /* "cvs import" shouldn't check CVS/Root; in general it
	       ignores CVS directories and CVS/Root is likely to
	       specify a different repository than the one we are
	       importing to.  */

	    if (lookup_command_attribute (command_name)
                & CVS_CMD_IGNORE_ADMROOT)
            {
		CVSADM_Root = Name_Root((char *) NULL, (char *) NULL);
            }

	    if (CVSADM_Root != NULL)
	    {
		if (CVSroot == NULL || !cvs_update_env)
		{
		    CVSroot = CVSADM_Root;
		    cvs_update_env = 1;	/* need to update environment */
		}
		/* Let -d override CVS/Root file.  The user might want
		   to change the access method, use a different server
		   (if there are two server machines which share the
		   repository using a networked file system), etc.  */
		else if (
#ifdef CLIENT_SUPPORT
		         !getenv ("CVS_IGNORE_REMOTE_ROOT") &&
#endif
			 strcmp (CVSroot, CVSADM_Root) != 0)
		{
		    /* Once we have verified that this root is usable,
		       we will want to write it into CVS/Root.

		       Don't do it for the "login" command, however.
		       Consider: if the user executes "cvs login" with
		       the working directory inside an already checked
		       out module, we'd incorrectly change the
		       CVS/Root file to reflect the CVSROOT of the
		       "cvs login" command.  Ahh, the things one
		       discovers. */

		    if (lookup_command_attribute (command_name)
                        & CVS_CMD_USES_WORK_DIR)
                    {
			need_to_create_root = 1;
                    }

		}
	    }

	    /* Now we've reconciled CVSROOT from the command line, the
               CVS/Root file, and the environment variable.  Do the
               last sanity checks on the variable. */

	    if (! CVSroot)
	    {
		error (0, 0,
		       "No CVSROOT specified!  Please use the `-d' option");
		error (1, 0,
		       "or set the %s environment variable.", CVSROOT_ENV);
	    }
	    
	    if (! *CVSroot)
	    {
		error (0, 0,
		       "CVSROOT is set but empty!  Make sure that the");
		error (0, 0,
		       "specification of CVSROOT is legal, either via the");
		error (0, 0,
		       "`-d' option, the %s environment variable, or the",
		       CVSROOT_ENV);
		error (1, 0,
		       "CVS/Root file (if any).");
	    }

	    /* Now we're 100% sure that we have a valid CVSROOT
	       variable.  Parse it to see if we're supposed to do
	       remote accesses or use a special access method. */

	    if (parse_cvsroot (CVSroot))
		error (1, 0, "Bad CVSROOT.");

	    /*
	     * Check to see if we can write into the history file.  If not,
	     * we assume that we can't work in the repository.
	     * BUT, only if the history file exists.
	     */

	    if (!client_active)
	    {
		char *path;
		int save_errno;

		path = xmalloc (strlen (CVSroot_directory)
				+ sizeof (CVSROOTADM)
				+ 20
				+ sizeof (CVSROOTADM_HISTORY));
		(void) sprintf (path, "%s/%s", CVSroot_directory, CVSROOTADM);
		if (!isaccessible (path, R_OK | X_OK))
		{
		    save_errno = errno;
		    /* If this is "cvs init", the root need not exist yet.  */
		    if (strcmp (command_name, "init") != 0)
		    {
			error (1, save_errno, "%s", path);
		    }
		}
		(void) strcat (path, "/");
		(void) strcat (path, CVSROOTADM_HISTORY);
		if (isfile (path) && !isaccessible (path, R_OK | W_OK))
		{
		    save_errno = errno;
		    error (0, 0, "Sorry, you don't have read/write access to the history file");
		    error (1, save_errno, "%s", path);
		}
		free (path);
	    }

#ifdef HAVE_PUTENV
	    /* Update the CVSROOT environment variable if necessary. */

	    if (cvs_update_env)
	    {
		char *env;
		env = xmalloc (strlen (CVSROOT_ENV) + strlen (CVSroot)
			       + 1 + 1);
		(void) sprintf (env, "%s=%s", CVSROOT_ENV, CVSroot);
		(void) putenv (env);
		/* do not free env, as putenv has control of it */
	    }
#endif
	}
	
	/* This is only used for writing into the history file.  For
	   remote connections, it might be nice to have hostname
	   and/or remote path, on the other hand I'm not sure whether
	   it is worth the trouble.  */

#ifdef SERVER_SUPPORT
	if (server_active)
	    CurDir = xstrdup ("<remote>");
	else
#endif
	{
	    CurDir = xgetwd ();
            if (CurDir == NULL)
		error (1, errno, "cannot get working directory");
	}

	if (Tmpdir == NULL || Tmpdir[0] == '\0')
	    Tmpdir = "/tmp";

#ifdef HAVE_PUTENV
	if (tmpdir_update_env)
	{
	    char *env;
	    env = xmalloc (strlen (TMPDIR_ENV) + strlen (Tmpdir) + 1 + 1);
	    (void) sprintf (env, "%s=%s", TMPDIR_ENV, Tmpdir);
	    (void) putenv (env);
	    /* do not free env, as putenv has control of it */
	}
#endif

#ifndef DONT_USE_SIGNALS
	/* make sure we clean up on error */
#ifdef SIGHUP
	(void) SIG_register (SIGHUP, main_cleanup);
	(void) SIG_register (SIGHUP, Lock_Cleanup);
#endif
#ifdef SIGINT
	(void) SIG_register (SIGINT, main_cleanup);
	(void) SIG_register (SIGINT, Lock_Cleanup);
#endif
#ifdef SIGQUIT
	(void) SIG_register (SIGQUIT, main_cleanup);
	(void) SIG_register (SIGQUIT, Lock_Cleanup);
#endif
#ifdef SIGPIPE
	(void) SIG_register (SIGPIPE, main_cleanup);
	(void) SIG_register (SIGPIPE, Lock_Cleanup);
#endif
#ifdef SIGTERM
	(void) SIG_register (SIGTERM, main_cleanup);
	(void) SIG_register (SIGTERM, Lock_Cleanup);
#endif
#endif /* !DONT_USE_SIGNALS */

	gethostname(hostname, sizeof (hostname));

#ifdef KLUDGE_FOR_WNT_TESTSUITE
	/* Probably the need for this will go away at some point once
	   we call fflush enough places (e.g. fflush (stdout) in
	   cvs_outerr).  */
	(void) setvbuf (stdout, (char *) NULL, _IONBF, 0);
	(void) setvbuf (stderr, (char *) NULL, _IONBF, 0);
#endif /* KLUDGE_FOR_WNT_TESTSUITE */

	if (use_cvsrc)
	    read_cvsrc (&argc, &argv, command_name);

	/* Parse the CVSROOT/config file, but only for local.  For the
	   server, we parse it after we know $CVSROOT.  For the
	   client, it doesn't get parsed at all, obviously.  The
	   presence of the parse_config call here is not mean to
	   predetermine whether CVSROOT/config overrides things from
	   read_cvsrc and other such places or vice versa.  That sort
	   of thing probably needs more thought.  */
	if (1
#ifdef SERVER_SUPPORT
	    && !server_active
#endif
#ifdef CLIENT_SUPPORT
	    && !client_active
#endif
	    )
	{
	    /* If there was an error parsing the config file, parse_config
	       already printed an error.  We keep going.  Why?  Because
	       if we didn't, then there would be no way to check in a new
	       CVSROOT/config file to fix the broken one!  */
	    parse_config (CVSroot_directory);
	}
    } /* end of stuff that gets done if the user DOESN'T ask for help */

    err = (*(cm->func)) (argc, argv);

    if (need_to_create_root)
    {
	/* Update the CVS/Root file.  We might want to do this in
	   all directories that we recurse into, but currently we
	   don't.  Note that if there is an error writing the file,
	   we give an error/warning.  This is so if users try to rewrite
	   CVS/Root with the -d option (a documented feature), they will
	   either succeed, or be told why it didn't work.  */
	Create_Root (NULL, CVSroot);
    }

    Lock_Cleanup ();

    free (program_path);
    if (free_CVSroot)
	free (CVSroot);
    if (free_Editor)
	free (Editor);
    if (free_Tmpdir)
	free (Tmpdir);
    root_allow_free ();

#ifdef SYSTEM_CLEANUP
    /* Hook for OS-specific behavior, for example socket subsystems on
       NT and OS2 or dealing with windows and arguments on Mac.  */
    SYSTEM_CLEANUP ();
#endif

    /* This is exit rather than return because apparently that keeps
       some tools which check for memory leaks happier.  */
    exit (err ? EXIT_FAILURE : 0);
	/* Keep picky/stupid compilers (e.g. Visual C++ 5.0) happy.  */
	return 0;
}

char *
Make_Date (rawdate)
    char *rawdate;
{
    time_t unixtime;

    unixtime = get_date (rawdate, (struct timeb *) NULL);
    if (unixtime == (time_t) - 1)
	error (1, 0, "Can't parse date/time: %s", rawdate);
    return date_from_time_t (unixtime);
}

/* Convert a time_t to an RCS format date.  This is mainly for the
   use of "cvs history", because the CVSROOT/history file contains
   time_t format dates; most parts of CVS will want to avoid using
   time_t's directly, and instead use RCS_datecmp, Make_Date, &c.
   Assuming that the time_t is in GMT (as it generally should be),
   then the result will be in GMT too.

   Returns a newly malloc'd string.  */

char *
date_from_time_t (unixtime)
    time_t unixtime;
{
    struct tm *ftm;
    char date[MAXDATELEN];
    char *ret;

    ftm = gmtime (&unixtime);
    if (ftm == NULL)
	/* This is a system, like VMS, where the system clock is in local
	   time.  Hopefully using localtime here matches the "zero timezone"
	   hack I added to get_date (get_date of course being the relevant
	   issue for Make_Date, and for history.c too I think).  */
	ftm = localtime (&unixtime);

    (void) sprintf (date, DATEFORM,
		    ftm->tm_year + (ftm->tm_year < 100 ? 0 : 1900),
		    ftm->tm_mon + 1, ftm->tm_mday, ftm->tm_hour,
		    ftm->tm_min, ftm->tm_sec);
    ret = xstrdup (date);
    return (ret);
}

void
usage (cpp)
    register const char *const *cpp;
{
    (void) fprintf (stderr, *cpp++, program_name, command_name);
    for (; *cpp; cpp++)
	(void) fprintf (stderr, *cpp);
    error_exit ();
}
