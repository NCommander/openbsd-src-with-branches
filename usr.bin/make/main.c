/*	$NetBSD: main.c,v 1.24 1996/02/04 22:20:49 christos Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
 * Copyright (c) 1988, 1989 by Adam de Boor
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1989 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)main.c	5.25 (Berkeley) 4/1/91";
#else
static char rcsid[] = "$NetBSD: main.c,v 1.24 1996/02/04 22:20:49 christos Exp $";
#endif
#endif /* not lint */

/*-
 * main.c --
 *	The main file for this entire program. Exit routines etc
 *	reside here.
 *
 * Utility functions defined in this file:
 *	Main_ParseArgLine	Takes a line of arguments, breaks them and
 *				treats them as if they were given when first
 *				invoked. Used by the parse module to implement
 *				the .MFLAGS target.
 *
 *	Error			Print a tagged error message. The global
 *				MAKE variable must have been defined. This
 *				takes a format string and two optional
 *				arguments for it.
 *
 *	Fatal			Print an error message and exit. Also takes
 *				a format string and two arguments.
 *
 *	Punt			Aborts all jobs and exits with a message. Also
 *				takes a format string and two arguments.
 *
 *	Finish			Finish things up by printing the number of
 *				errors which occured, as passed to it, and
 *				exiting.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/resource.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include "make.h"
#include "hash.h"
#include "dir.h"
#include "job.h"
#include "pathnames.h"

#ifndef	DEFMAXLOCAL
#define	DEFMAXLOCAL DEFMAXJOBS
#endif	/* DEFMAXLOCAL */

#define	MAKEFLAGS	".MAKEFLAGS"

Lst			create;		/* Targets to be made */
time_t			now;		/* Time at start of make */
GNode			*DEFAULT;	/* .DEFAULT node */
Boolean			allPrecious;	/* .PRECIOUS given on line by itself */

static Boolean		noBuiltins;	/* -r flag */
static Lst		makefiles;	/* ordered list of makefiles to read */
int			maxJobs;	/* -j argument */
static int		maxLocal;	/* -L argument */
Boolean			compatMake;	/* -B argument */
Boolean			debug;		/* -d flag */
Boolean			noExecute;	/* -n flag */
Boolean			keepgoing;	/* -k flag */
Boolean			queryFlag;	/* -q flag */
Boolean			touchFlag;	/* -t flag */
Boolean			usePipes;	/* !-P flag */
Boolean			ignoreErrors;	/* -i flag */
Boolean			beSilent;	/* -s flag */
Boolean			oldVars;	/* variable substitution style */
Boolean			checkEnvFirst;	/* -e flag */
static Boolean		jobsRunning;	/* TRUE if the jobs might be running */

static Boolean		ReadMakefile();
static void		usage();

static char *curdir;			/* startup directory */
static char *objdir;			/* where we chdir'ed to */

/*-
 * MainParseArgs --
 *	Parse a given argument vector. Called from main() and from
 *	Main_ParseArgLine() when the .MAKEFLAGS target is used.
 *
 *	XXX: Deal with command line overriding .MAKEFLAGS in makefile
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Various global and local flags will be set depending on the flags
 *	given
 */
static void
MainParseArgs(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	extern char *optarg;
	int c;
	int forceJobs = 0;

	optind = 1;	/* since we're called more than once */
#ifdef REMOTE
# define OPTFLAGS "BD:I:L:PSd:ef:ij:knqrst"
#else
# define OPTFLAGS "BD:I:PSd:ef:ij:knqrst"
#endif
rearg:	while((c = getopt(argc, argv, OPTFLAGS)) != EOF) {
		switch(c) {
		case 'D':
			Var_Set(optarg, "1", VAR_GLOBAL);
			Var_Append(MAKEFLAGS, "-D", VAR_GLOBAL);
			Var_Append(MAKEFLAGS, optarg, VAR_GLOBAL);
			break;
		case 'I':
			Parse_AddIncludeDir(optarg);
			Var_Append(MAKEFLAGS, "-I", VAR_GLOBAL);
			Var_Append(MAKEFLAGS, optarg, VAR_GLOBAL);
			break;
		case 'B':
			compatMake = TRUE;
			break;
#ifdef REMOTE
		case 'L':
			maxLocal = atoi(optarg);
			Var_Append(MAKEFLAGS, "-L", VAR_GLOBAL);
			Var_Append(MAKEFLAGS, optarg, VAR_GLOBAL);
			break;
#endif
		case 'P':
			usePipes = FALSE;
			Var_Append(MAKEFLAGS, "-P", VAR_GLOBAL);
			break;
		case 'S':
			keepgoing = FALSE;
			Var_Append(MAKEFLAGS, "-S", VAR_GLOBAL);
			break;
		case 'd': {
			char *modules = optarg;

			for (; *modules; ++modules)
				switch (*modules) {
				case 'A':
					debug = ~0;
					break;
				case 'a':
					debug |= DEBUG_ARCH;
					break;
				case 'c':
					debug |= DEBUG_COND;
					break;
				case 'd':
					debug |= DEBUG_DIR;
					break;
				case 'f':
					debug |= DEBUG_FOR;
					break;
				case 'g':
					if (modules[1] == '1') {
						debug |= DEBUG_GRAPH1;
						++modules;
					}
					else if (modules[1] == '2') {
						debug |= DEBUG_GRAPH2;
						++modules;
					}
					break;
				case 'j':
					debug |= DEBUG_JOB;
					break;
				case 'm':
					debug |= DEBUG_MAKE;
					break;
				case 's':
					debug |= DEBUG_SUFF;
					break;
				case 't':
					debug |= DEBUG_TARG;
					break;
				case 'v':
					debug |= DEBUG_VAR;
					break;
				default:
					(void)fprintf(stderr,
				"make: illegal argument to d option -- %c\n",
					    *modules);
					usage();
				}
			Var_Append(MAKEFLAGS, "-d", VAR_GLOBAL);
			Var_Append(MAKEFLAGS, optarg, VAR_GLOBAL);
			break;
		}
		case 'e':
			checkEnvFirst = TRUE;
			Var_Append(MAKEFLAGS, "-e", VAR_GLOBAL);
			break;
		case 'f':
			(void)Lst_AtEnd(makefiles, (ClientData)optarg);
			break;
		case 'i':
			ignoreErrors = TRUE;
			Var_Append(MAKEFLAGS, "-i", VAR_GLOBAL);
			break;
		case 'j':
			forceJobs = TRUE;
			maxJobs = atoi(optarg);
#ifndef REMOTE
			maxLocal = maxJobs;
#endif
			Var_Append(MAKEFLAGS, "-j", VAR_GLOBAL);
			Var_Append(MAKEFLAGS, optarg, VAR_GLOBAL);
			break;
		case 'k':
			keepgoing = TRUE;
			Var_Append(MAKEFLAGS, "-k", VAR_GLOBAL);
			break;
		case 'n':
			noExecute = TRUE;
			Var_Append(MAKEFLAGS, "-n", VAR_GLOBAL);
			break;
		case 'q':
			queryFlag = TRUE;
			/* Kind of nonsensical, wot? */
			Var_Append(MAKEFLAGS, "-q", VAR_GLOBAL);
			break;
		case 'r':
			noBuiltins = TRUE;
			Var_Append(MAKEFLAGS, "-r", VAR_GLOBAL);
			break;
		case 's':
			beSilent = TRUE;
			Var_Append(MAKEFLAGS, "-s", VAR_GLOBAL);
			break;
		case 't':
			touchFlag = TRUE;
			Var_Append(MAKEFLAGS, "-t", VAR_GLOBAL);
			break;
		default:
		case '?':
			usage();
		}
	}

	/*
	 * Be compatible if user did not specify -j and did not explicitly
	 * turned compatibility on
	 */
	if (!compatMake && !forceJobs)
		compatMake = TRUE;

	oldVars = TRUE;

	/*
	 * See if the rest of the arguments are variable assignments and
	 * perform them if so. Else take them to be targets and stuff them
	 * on the end of the "create" list.
	 */
	for (argv += optind, argc -= optind; *argv; ++argv, --argc)
		if (Parse_IsVar(*argv)) {
			char *var = strdup(*argv);

			Parse_DoVar(var, VAR_CMD);
			free(var);
		} else {
			if (!**argv)
				Punt("illegal (null) argument.");
			if (**argv == '-') {
				if ((*argv)[1])
					optind = 0;     /* -flag... */
				else
					optind = 1;     /* - */
				goto rearg;
			}
			(void)Lst_AtEnd(create, (ClientData)strdup(*argv));
		}
}

/*-
 * Main_ParseArgLine --
 *  	Used by the parse module when a .MFLAGS or .MAKEFLAGS target
 *	is encountered and by main() when reading the .MAKEFLAGS envariable.
 *	Takes a line of arguments and breaks it into its
 * 	component words and passes those words and the number of them to the
 *	MainParseArgs function.
 *	The line should have all its leading whitespace removed.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	Only those that come from the various arguments.
 */
void
Main_ParseArgLine(line)
	char *line;			/* Line to fracture */
{
	char **argv;			/* Manufactured argument vector */
	int argc;			/* Number of arguments in argv */

	if (line == NULL)
		return;
	for (; *line == ' '; ++line)
		continue;
	if (!*line)
		return;

	argv = brk_string(line, &argc, TRUE);
	MainParseArgs(argc, argv);
}

/*-
 * main --
 *	The main function, for obvious reasons. Initializes variables
 *	and a few modules, then parses the arguments give it in the
 *	environment and on the command line. Reads the system makefile
 *	followed by either Makefile, makefile or the file given by the
 *	-f argument. Sets the .MAKEFLAGS PMake variable based on all the
 *	flags it has received by then uses either the Make or the Compat
 *	module to create the initial list of targets.
 *
 * Results:
 *	If -q was given, exits -1 if anything was out-of-date. Else it exits
 *	0.
 *
 * Side Effects:
 *	The program exits when done. Targets are created. etc. etc. etc.
 */
int
main(argc, argv)
	int argc;
	char **argv;
{
	Lst targs;	/* target nodes to create -- passed to Make_Init */
	Boolean outOfDate = TRUE; 	/* FALSE if all targets up to date */
	struct stat sb, sa;
	char *p, *p1, *path, *pwd, *getenv(), *getwd();
	char mdpath[MAXPATHLEN + 1];
	char obpath[MAXPATHLEN + 1];
	char cdpath[MAXPATHLEN + 1];
	struct utsname utsname;
    	char *machine = getenv("MACHINE");

#ifdef RLIMIT_NOFILE
	/*
	 * get rid of resource limit on file descriptors
	 */
	{
		struct rlimit rl;
		if (getrlimit(RLIMIT_NOFILE, &rl) != -1 &&
		    rl.rlim_cur != rl.rlim_max) {
			rl.rlim_cur = rl.rlim_max;
			(void) setrlimit(RLIMIT_NOFILE, &rl);
		}
	}
#endif
	/*
	 * Find where we are and take care of PWD for the automounter...
	 * All this code is so that we know where we are when we start up
	 * on a different machine with pmake.
	 */
	curdir = cdpath;
	if (getcwd(curdir, MAXPATHLEN) == NULL) {
		(void)fprintf(stderr, "make: %s.\n", strerror(errno));
		exit(2);
	}

	if (stat(curdir, &sa) == -1) {
	    (void)fprintf(stderr, "make: %s: %s.\n",
			  curdir, strerror(errno));
	    exit(2);
	}

	if ((pwd = getenv("PWD")) != NULL) {
	    if (stat(pwd, &sb) == 0 && sa.st_ino == sb.st_ino &&
		sa.st_dev == sb.st_dev) 
		(void) strcpy(curdir, pwd);
	}

	/*
	 * Get the name of this type of MACHINE from utsname
	 * so we can share an executable for similar machines.
	 * (i.e. m68k: amiga hp300, mac68k, sun3, ...)
	 *
	 * Note that while MACHINE is decided at run-time,
	 * MACHINE_ARCH is always known at compile time.
	 */
    	if (!machine) {
	    if (uname(&utsname) == -1) {
		    perror("make: uname");
		    exit(2);
	    }
	    machine = utsname.machine;
	}

	/*
	 * if the MAKEOBJDIR (or by default, the _PATH_OBJDIR) directory
	 * exists, change into it and build there.  Once things are
	 * initted, have to add the original directory to the search path,
	 * and modify the paths for the Makefiles apropriately.  The
	 * current directory is also placed as a variable for make scripts.
	 */
	if (!(path = getenv("MAKEOBJDIR"))) {
		path = _PATH_OBJDIR;
		(void) sprintf(mdpath, "%s.%s", path, machine);
	}
	else
		(void) strncpy(mdpath, path, MAXPATHLEN + 1);
	
	if (stat(mdpath, &sb) == 0 && S_ISDIR(sb.st_mode)) {

		if (chdir(mdpath)) {
			(void)fprintf(stderr, "make warning: %s: %s.\n",
				      mdpath, strerror(errno));
			objdir = curdir;
		}
		else {
			if (mdpath[0] != '/') {
				(void) sprintf(obpath, "%s/%s", curdir, mdpath);
				objdir = obpath;
			}
			else
				objdir = mdpath;
		}
	}
	else {
		if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)) {

			if (chdir(path)) {
				(void)fprintf(stderr, "make warning: %s: %s.\n",
					      path, strerror(errno));
				objdir = curdir;
			}
			else {
				if (path[0] != '/') {
					(void) sprintf(obpath, "%s/%s", curdir,
						       path);
					objdir = obpath;
				}
				else
					objdir = obpath;
			}
		}
		else
			objdir = curdir;
	}

	setenv("PWD", objdir, 1);

	create = Lst_Init(FALSE);
	makefiles = Lst_Init(FALSE);
	beSilent = FALSE;		/* Print commands as executed */
	ignoreErrors = FALSE;		/* Pay attention to non-zero returns */
	noExecute = FALSE;		/* Execute all commands */
	keepgoing = FALSE;		/* Stop on error */
	allPrecious = FALSE;		/* Remove targets when interrupted */
	queryFlag = FALSE;		/* This is not just a check-run */
	noBuiltins = FALSE;		/* Read the built-in rules */
	touchFlag = FALSE;		/* Actually update targets */
	usePipes = TRUE;		/* Catch child output in pipes */
	debug = 0;			/* No debug verbosity, please. */
	jobsRunning = FALSE;

	maxLocal = DEFMAXLOCAL;		/* Set default local max concurrency */
#ifdef REMOTE
	maxJobs = DEFMAXJOBS;		/* Set default max concurrency */
#else
	maxJobs = maxLocal;
#endif
	compatMake = FALSE;		/* No compat mode */
    

	/*
	 * Initialize the parsing, directory and variable modules to prepare
	 * for the reading of inclusion paths and variable settings on the
	 * command line
	 */
	Dir_Init();		/* Initialize directory structures so -I flags
				 * can be processed correctly */
	Parse_Init();		/* Need to initialize the paths of #include
				 * directories */
	Var_Init();		/* As well as the lists of variables for
				 * parsing arguments */
        str_init();
	if (objdir != curdir)
		Dir_AddDir(dirSearchPath, curdir);
	Var_Set(".CURDIR", curdir, VAR_GLOBAL);
	Var_Set(".OBJDIR", objdir, VAR_GLOBAL);

	/*
	 * Initialize various variables.
	 *	MAKE also gets this name, for compatibility
	 *	.MAKEFLAGS gets set to the empty string just in case.
	 *	MFLAGS also gets initialized empty, for compatibility.
	 */
	Var_Set("MAKE", argv[0], VAR_GLOBAL);
	Var_Set(MAKEFLAGS, "", VAR_GLOBAL);
	Var_Set("MFLAGS", "", VAR_GLOBAL);
	Var_Set("MACHINE", machine, VAR_GLOBAL);
#ifdef MACHINE_ARCH
	Var_Set("MACHINE_ARCH", MACHINE_ARCH, VAR_GLOBAL);
#endif

	/*
	 * First snag any flags out of the MAKE environment variable.
	 * (Note this is *not* MAKEFLAGS since /bin/make uses that and it's
	 * in a different format).
	 */
#ifdef POSIX
	Main_ParseArgLine(getenv("MAKEFLAGS"));
#else
	Main_ParseArgLine(getenv("MAKE"));
#endif
    
	MainParseArgs(argc, argv);

	/*
	 * Initialize archive, target and suffix modules in preparation for
	 * parsing the makefile(s)
	 */
	Arch_Init();
	Targ_Init();
	Suff_Init();

	DEFAULT = NILGNODE;
	(void)time(&now);

	/*
	 * Set up the .TARGETS variable to contain the list of targets to be
	 * created. If none specified, make the variable empty -- the parser
	 * will fill the thing in with the default or .MAIN target.
	 */
	if (!Lst_IsEmpty(create)) {
		LstNode ln;

		for (ln = Lst_First(create); ln != NILLNODE;
		    ln = Lst_Succ(ln)) {
			char *name = (char *)Lst_Datum(ln);

			Var_Append(".TARGETS", name, VAR_GLOBAL);
		}
	} else
		Var_Set(".TARGETS", "", VAR_GLOBAL);

	/*
	 * Read in the built-in rules first, followed by the specified makefile,
	 * if it was (makefile != (char *) NULL), or the default Makefile and
	 * makefile, in that order, if it wasn't.
	 */
	 if (!noBuiltins && !ReadMakefile(_PATH_DEFSYSMK))
		Fatal("make: no system rules (%s).", _PATH_DEFSYSMK);

	if (!Lst_IsEmpty(makefiles)) {
		LstNode ln;

		ln = Lst_Find(makefiles, (ClientData)NULL, ReadMakefile);
		if (ln != NILLNODE)
			Fatal("make: cannot open %s.", (char *)Lst_Datum(ln));
	} else if (!ReadMakefile("makefile"))
		(void)ReadMakefile("Makefile");

	(void)ReadMakefile(".depend");

	Var_Append("MFLAGS", Var_Value(MAKEFLAGS, VAR_GLOBAL, &p1), VAR_GLOBAL);
	if (p1)
	    free(p1);

	/* Install all the flags into the MAKE envariable. */
	if (((p = Var_Value(MAKEFLAGS, VAR_GLOBAL, &p1)) != NULL) && *p)
#ifdef POSIX
		setenv("MAKEFLAGS", p, 1);
#else
		setenv("MAKE", p, 1);
#endif
	if (p1)
	    free(p1);

	/*
	 * For compatibility, look at the directories in the VPATH variable
	 * and add them to the search path, if the variable is defined. The
	 * variable's value is in the same format as the PATH envariable, i.e.
	 * <directory>:<directory>:<directory>...
	 */
	if (Var_Exists("VPATH", VAR_CMD)) {
		char *vpath, *path, *cp, savec;
		/*
		 * GCC stores string constants in read-only memory, but
		 * Var_Subst will want to write this thing, so store it
		 * in an array
		 */
		static char VPATH[] = "${VPATH}";

		vpath = Var_Subst(NULL, VPATH, VAR_CMD, FALSE);
		path = vpath;
		do {
			/* skip to end of directory */
			for (cp = path; *cp != ':' && *cp != '\0'; cp++)
				continue;
			/* Save terminator character so know when to stop */
			savec = *cp;
			*cp = '\0';
			/* Add directory to search path */
			Dir_AddDir(dirSearchPath, path);
			*cp = savec;
			path = cp + 1;
		} while (savec == ':');
		(void)free((Address)vpath);
	}

	/*
	 * Now that all search paths have been read for suffixes et al, it's
	 * time to add the default search path to their lists...
	 */
	Suff_DoPaths();

	/* print the initial graph, if the user requested it */
	if (DEBUG(GRAPH1))
		Targ_PrintGraph(1);

	/*
	 * Have now read the entire graph and need to make a list of targets
	 * to create. If none was given on the command line, we consult the
	 * parsing module to find the main target(s) to create.
	 */
	if (Lst_IsEmpty(create))
		targs = Parse_MainName();
	else
		targs = Targ_FindList(create, TARG_CREATE);

	if (!compatMake) {
		/*
		 * Initialize job module before traversing the graph, now that
		 * any .BEGIN and .END targets have been read.  This is done
		 * only if the -q flag wasn't given (to prevent the .BEGIN from
		 * being executed should it exist).
		 */
		if (!queryFlag) {
			if (maxLocal == -1)
				maxLocal = maxJobs;
			Job_Init(maxJobs, maxLocal);
			jobsRunning = TRUE;
		}

		/* Traverse the graph, checking on all the targets */
		outOfDate = Make_Run(targs);
	} else
		/*
		 * Compat_Init will take care of creating all the targets as
		 * well as initializing the module.
		 */
		Compat_Run(targs);
    
	Lst_Destroy(targs, NOFREE);
	Lst_Destroy(makefiles, NOFREE);
	Lst_Destroy(create, (void (*) __P((ClientData))) free);

	/* print the graph now it's been processed if the user requested it */
	if (DEBUG(GRAPH2))
		Targ_PrintGraph(2);

	Suff_End();
        Targ_End();
	Arch_End();
	str_end();
	Var_End();
	Parse_End();
	Dir_End();

	if (queryFlag && outOfDate)
		return(1);
	else
		return(0);
}

/*-
 * ReadMakefile  --
 *	Open and parse the given makefile.
 *
 * Results:
 *	TRUE if ok. FALSE if couldn't open file.
 *
 * Side Effects:
 *	lots
 */
static Boolean
ReadMakefile(fname)
	char *fname;		/* makefile to read */
{
	extern Lst parseIncPath, sysIncPath;
	FILE *stream;
	char *name, path[MAXPATHLEN + 1];

	if (!strcmp(fname, "-")) {
		Parse_File("(stdin)", stdin);
		Var_Set("MAKEFILE", "", VAR_GLOBAL);
	} else {
		if ((stream = fopen(fname, "r")) != NULL)
			goto found;
		/* if we've chdir'd, rebuild the path name */
		if (curdir != objdir && *fname != '/') {
			(void)sprintf(path, "%s/%s", curdir, fname);
			if ((stream = fopen(path, "r")) != NULL) {
				fname = path;
				goto found;
			}
		}
		/* look in -I and system include directories. */
		name = Dir_FindFile(fname, parseIncPath);
		if (!name)
			name = Dir_FindFile(fname, sysIncPath);
		if (!name || !(stream = fopen(name, "r")))
			return(FALSE);
		fname = name;
		/*
		 * set the MAKEFILE variable desired by System V fans -- the
		 * placement of the setting here means it gets set to the last
		 * makefile specified, as it is set by SysV make.
		 */
found:		Var_Set("MAKEFILE", fname, VAR_GLOBAL);
		Parse_File(fname, stream);
		(void)fclose(stream);
	}
	return(TRUE);
}

/*-
 * Error --
 *	Print an error message given its format.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The message is printed.
 */
/* VARARGS */
void
#if __STDC__
Error(char *fmt, ...)
#else
Error(va_alist)
	va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	char *fmt;

	va_start(ap);
	fmt = va_arg(ap, char *);
#endif
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	(void)fflush(stderr);
}

/*-
 * Fatal --
 *	Produce a Fatal error message. If jobs are running, waits for them
 *	to finish.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	The program exits
 */
/* VARARGS */
void
#if __STDC__
Fatal(char *fmt, ...)
#else
Fatal(va_alist)
	va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	char *fmt;

	va_start(ap);
	fmt = va_arg(ap, char *);
#endif
	if (jobsRunning)
		Job_Wait();

	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	(void)fflush(stderr);

	if (DEBUG(GRAPH2))
		Targ_PrintGraph(2);
	exit(2);		/* Not 1 so -q can distinguish error */
}

/*
 * Punt --
 *	Major exception once jobs are being created. Kills all jobs, prints
 *	a message and exits.
 *
 * Results:
 *	None 
 *
 * Side Effects:
 *	All children are killed indiscriminately and the program Lib_Exits
 */
/* VARARGS */
void
#if __STDC__
Punt(char *fmt, ...)
#else
Punt(va_alist)
	va_dcl
#endif
{
	va_list ap;
#if __STDC__
	va_start(ap, fmt);
#else
	char *fmt;

	va_start(ap);
	fmt = va_arg(ap, char *);
#endif

	(void)fprintf(stderr, "make: ");
	(void)vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, "\n");
	(void)fflush(stderr);

	DieHorribly();
}

/*-
 * DieHorribly --
 *	Exit without giving a message.
 *
 * Results:
 *	None
 *
 * Side Effects:
 *	A big one...
 */
void
DieHorribly()
{
	if (jobsRunning)
		Job_AbortAll();
	if (DEBUG(GRAPH2))
		Targ_PrintGraph(2);
	exit(2);		/* Not 1, so -q can distinguish error */
}

/*
 * Finish --
 *	Called when aborting due to errors in child shell to signal
 *	abnormal exit. 
 *
 * Results:
 *	None 
 *
 * Side Effects:
 *	The program exits
 */
void
Finish(errors)
	int errors;	/* number of errors encountered in Make_Make */
{
	Fatal("%d error%s", errors, errors == 1 ? "" : "s");
}

/*
 * emalloc --
 *	malloc, but die on error.
 */
char *
emalloc(len)
	size_t len;
{
	char *p;

	if ((p = (char *) malloc(len)) == NULL)
		enomem();
	return(p);
}

/*
 * enomem --
 *	die when out of memory.
 */
void
enomem()
{
	(void)fprintf(stderr, "make: %s.\n", strerror(errno));
	exit(2);
}

/*
 * enunlink --
 *	Remove a file carefully, avoiding directories.
 */
int
eunlink(file)
	const char *file;
{
	struct stat st;

	if (lstat(file, &st) == -1)
		return -1;

	if (S_ISDIR(st.st_mode)) {
		errno = EISDIR;
		return -1;
	}
	return unlink(file);
}

/*
 * usage --
 *	exit with usage message
 */
static void
usage()
{
	(void)fprintf(stderr,
"usage: make [-eiknqrst] [-D variable] [-d flags] [-f makefile ]\n\
            [-I directory] [-j max_jobs] [variable=value]\n");
	exit(2);
}


int
PrintAddr(a, b)
    ClientData a;
    ClientData b;
{
    printf("%lx ", (unsigned long) a);
    return b ? 0 : 0;
}
