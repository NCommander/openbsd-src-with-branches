#define DEBUG
/*	$OpenBSD$	*/
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
#include <sys/wait.h>

#include <err.h>
#include <pwd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sysexits.h>

#include "cvs.h"
#include "log.h"


extern char *__progname;


/* verbosity level: 0 = really quiet, 1 = quiet, 2 = verbose */
int verbosity = 2;



/* compression level used with zlib, 0 meaning no compression taking place */
int   cvs_compress = 0;
int   cvs_trace = 0;
int   cvs_nolog = 0;
int   cvs_readonly = 0;

/* name of the command we are running */
char *cvs_command;
char *cvs_rootstr;
char *cvs_rsh = CVS_RSH_DEFAULT;
char *cvs_editor = CVS_EDITOR_DEFAULT;

struct cvsroot *cvs_root = NULL;


/*
 * Command dispatch table
 * ----------------------
 *
 * The synopsis field should only contain the list of arguments that the
 * command supports, without the actual command's name.
 *
 * Command handlers are expected to return 0 if no error occured, or one of
 * the values known in sysexits.h in case of an error.  In case the error
 * returned is EX_USAGE, the command's usage string is printed to standard
 * error before returning.
 */

static struct cvs_cmd {
	char    cmd_name[CVS_CMD_MAXNAMELEN];
	char    cmd_alias[CVS_CMD_MAXALIAS][CVS_CMD_MAXNAMELEN];
	int   (*cmd_hdlr)(int, char **);
	char   *cmd_synopsis;
	char    cmd_descr[CVS_CMD_MAXDESCRLEN];
} cvs_cdt[] = {
	{
		"add",      { "ad",  "new" }, cvs_add,
		"[-m msg] file ...",
		"Add a new file/directory to the repository",
	},
	{
		"admin",    { "adm", "rcs" }, NULL,
		"",
		"Administration front end for rcs",
	},
	{
		"annotate", { "ann"        }, NULL,
		"",
		"Show last revision where each line was modified",
	},
	{
		"checkout", { "co",  "get" }, cvs_checkout,
		"",
		"Checkout sources for editing",
	},
	{
		"commit",   { "ci",  "com" }, cvs_commit,
		"[-flR] [-F logfile | -m msg] [-r rev] ...",
		"Check files into the repository",
	},
	{
		"diff",     { "di",  "dif" }, cvs_diff,
		"[-cilu] [-D date] [-r rev] ...",
		"Show differences between revisions",
	},
	{
		"edit",     {              }, NULL,
		"",
		"Get ready to edit a watched file",
	},
	{
		"editors",  {              }, NULL,
		"",
		"See who is editing a watched file",
	},
	{
		"export",   { "ex",  "exp" }, NULL,
		"",
		"Export sources from CVS, similar to checkout",
	},
	{
		"history",  { "hi",  "his" }, cvs_history,
		"",
		"Show repository access history",
	},
	{
		"import",   { "im",  "imp" }, NULL,
		"",
		"Import sources into CVS, using vendor branches",
	},
	{
		"init",     {              }, cvs_init,
		"",
		"Create a CVS repository if it doesn't exist",
	},
#if defined(HAVE_KERBEROS)
	{
		"kserver",  {}, NULL
		"",
		"Start a Kerberos authentication CVS server",
	},
#endif
	{
		"log",      { "lo"         }, cvs_getlog,
		"",
		"Print out history information for files",
	},
	{
		"login",    {}, NULL,
		"",
		"Prompt for password for authenticating server",
	},
	{
		"logout",   {}, NULL,
		"",
		"Removes entry in .cvspass for remote repository",
	},
	{
		"rdiff",    {}, NULL,
		"",
		"Create 'patch' format diffs between releases",
	},
	{
		"release",  {}, NULL,
		"",
		"Indicate that a Module is no longer in use",
	},
	{
		"remove",   {}, NULL,
		"",
		"Remove an entry from the repository",
	},
	{
		"rlog",     {}, NULL,
		"",
		"Print out history information for a module",
	},
	{
		"rtag",     {}, NULL,
		"",
		"Add a symbolic tag to a module",
	},
	{
		"server",   {}, cvs_server,
		"",
		"Server mode",
	},
	{
		"status",   {}, NULL,
		"",
		"Display status information on checked out files",
	},
	{
		"tag",      { "ta", }, NULL,
		"",
		"Add a symbolic tag to checked out version of files",
	},
	{
		"unedit",   {}, NULL,
		"",
		"Undo an edit command",
	},
	{
		"update",   {}, cvs_update,
		"",
		"Bring work tree in sync with repository",
	},
	{
		"version",  {}, cvs_version,
		"",
		"Show current CVS version(s)",
	},
	{
		"watch",    {}, NULL,
		"",
		"Set watches",
	},
	{
		"watchers", {}, NULL,
		"",
		"See who is watching a file",
	},
};

#define CVS_NBCMD  (sizeof(cvs_cdt)/sizeof(cvs_cdt[0]))



void             usage        (void);
void             sigchld_hdlr (int);
void             cvs_readrc   (void);
struct cvs_cmd*  cvs_findcmd  (const char *); 



/*
 * sigchld_hdlr()
 *
 * Handler for the SIGCHLD signal, which can be received in case we are
 * running a remote server and it dies.
 */

void
sigchld_hdlr(int signo)
{
	int status;
	pid_t pid;

	if ((pid = wait(&status)) == -1) {
	}
}


/*
 * usage()
 *
 * Display usage information.
 */

void
usage(void)
{
	fprintf(stderr,
	    "Usage: %s [-lQqtv] [-d root] [-e editor] [-z level] "
	    "command [options] ...\n",
	    __progname);
}


int
main(int argc, char **argv)
{
	char *envstr, *ep;
	int ret;
	u_int i, readrc;
	struct cvs_cmd *cmdp;

	readrc = 1;

	if (cvs_log_init(LD_STD, 0) < 0)
		err(1, "failed to initialize logging");

	/* by default, be very verbose */
	(void)cvs_log_filter(LP_FILTER_UNSET, LP_INFO);

#ifdef DEBUG
	(void)cvs_log_filter(LP_FILTER_UNSET, LP_DEBUG);
#endif

	/* check environment so command-line options override it */
	if ((envstr = getenv("CVS_RSH")) != NULL)
		cvs_rsh = envstr;

	if (((envstr = getenv("CVSEDITOR")) != NULL) ||
	    ((envstr = getenv("VISUAL")) != NULL) ||
	    ((envstr = getenv("EDITOR")) != NULL))
		cvs_editor = envstr;

	while ((ret = getopt(argc, argv, "d:e:fHlnQqrtvz:")) != -1) {
		switch (ret) {
		case 'd':
			cvs_rootstr = optarg;
			break;
		case 'e':
			cvs_editor = optarg;
			break;
		case 'f':
			readrc = 0;
			break;
		case 'l':
			cvs_nolog = 1;
			break;
		case 'n':
			break;
		case 'Q':
			verbosity = 0;
			break;
		case 'q':
			/* don't override -Q */
			if (verbosity > 1)
				verbosity = 1;
			break;
		case 'r':
			cvs_readonly = 1;
			break;
		case 't':
			cvs_trace = 1;
			break;
		case 'v':
			printf("%s\n", CVS_VERSION);
			exit(0);
			/* NOTREACHED */
			break;
		case 'z':
			cvs_compress = (int)strtol(optarg, &ep, 10); 
			if (*ep != '\0')
				errx(1, "error parsing compression level");
			if (cvs_compress < 0 || cvs_compress > 9)
				errx(1, "gzip compression level must be "
				    "between 0 and 9");
			break;
		default:
			usage();
			exit(EX_USAGE);
		}
	}

	argc -= optind;
	argv += optind;

	/* reset getopt() for use by commands */
	optind = 1;
	optreset = 1;

	if (argc == 0) {
		usage();
		exit(EX_USAGE);
	}

	/* setup signal handlers */
	signal(SIGCHLD, sigchld_hdlr);

	cvs_file_init();

	if (readrc)
		cvs_readrc();

	cvs_command = argv[0];
	ret = -1;

	cmdp = cvs_findcmd(cvs_command);
	if (cmdp == NULL) {
		fprintf(stderr, "Unknown command: `%s'\n\n", cvs_command);
		fprintf(stderr, "CVS commands are:\n");
		for (i = 0; i < CVS_NBCMD; i++)
			fprintf(stderr, "\t%-16s%s\n",
			    cvs_cdt[i].cmd_name, cvs_cdt[i].cmd_descr);
		exit(EX_USAGE);
	}

	if (cmdp->cmd_hdlr == NULL) {
		cvs_log(LP_ERR, "command `%s' not implemented", cvs_command);
		exit(1);
	}

	ret = (*cmdp->cmd_hdlr)(argc, argv);
	if (ret == EX_USAGE) {
		fprintf(stderr, "Usage: %s %s %s\n", __progname, cvs_command,
		    cmdp->cmd_synopsis);
	}

	return (ret);
}


/*
 * cvs_findcmd()
 *
 * Find the entry in the command dispatch table whose name or one of its
 * aliases matches <cmd>.
 * Returns a pointer to the command entry on success, NULL on failure.
 */

struct cvs_cmd*
cvs_findcmd(const char *cmd)
{
	u_int i, j;
	struct cvs_cmd *cmdp;

	cmdp = NULL;

	for (i = 0; (i < CVS_NBCMD) && (cmdp == NULL); i++) {
		if (strcmp(cmd, cvs_cdt[i].cmd_name) == 0)
			cmdp = &cvs_cdt[i];
		else {
			for (j = 0; j < CVS_CMD_MAXALIAS; j++) {
				if (strcmp(cmd, cvs_cdt[i].cmd_alias[j]) == 0) {
					cmdp = &cvs_cdt[i];
					break;
				}
			}
		}
	}

	return (cmdp);
}


/*
 * cvs_readrc()
 *
 * Read the CVS `.cvsrc' file in the user's home directory.  If the file
 * exists, it should contain a list of arguments that should always be given
 * implicitly to the specified commands.
 */

void
cvs_readrc(void)
{
	char rcpath[MAXPATHLEN], linebuf[128], *lp;
	struct cvs_cmd *cmdp;
	struct passwd *pw;
	FILE *fp;

	pw = getpwuid(getuid());
	if (pw == NULL) {
		cvs_log(LP_NOTICE, "failed to get user's password entry");
		return;
	}

	snprintf(rcpath, sizeof(rcpath), "%s/%s", pw->pw_dir, CVS_PATH_RC);

	fp = fopen(rcpath, "r");
	if (fp == NULL) {
		if (errno != ENOENT)
			cvs_log(LP_NOTICE, "failed to open `%s': %s", rcpath,
			    strerror(errno));
		return;
	}

	while (fgets(linebuf, sizeof(linebuf), fp) != NULL) {
		lp = strchr(linebuf, ' ');

		/* ignore lines with no arguments */
		if (lp == NULL)
			continue;

		*(lp++) = '\0';
		if (strcmp(linebuf, "cvs") == 0) {
			/* global options */
		}
		else {
			cmdp = cvs_findcmd(linebuf);
			if (cmdp == NULL) {
				cvs_log(LP_NOTICE,
				    "unknown command `%s' in cvsrc",
				    linebuf);
				continue;
			}
		}
	}
	if (ferror(fp)) {
		cvs_log(LP_NOTICE, "failed to read line from cvsrc");
	}

	(void)fclose(fp);
}
