/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)cl_main.c	10.27 (Berkeley) 4/28/96";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <curses.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../common/common.h"
#include "cl.h"
#include "pathnames.h"

GS *__global_list;				/* GLOBAL: List of screens. */
sigset_t __sigblockset;				/* GLOBAL: Blocked signals. */

static GS	*gs_init __P((char *));
static void	 nomem __P((char *));
static int	 setsig __P((int, struct sigaction *, void (*)(int)));
static void	 sig_end __P((GS *));
static void	 term_init __P((char *, char *));

/*
 * main --
 *	This is the main loop for the standalone curses editor.
 */
int
main(argc, argv)
	int argc;
	char *argv[];
{
	static int reenter;
	CL_PRIVATE *clp;
	GS *gp;
	size_t rows, cols;
	int rval;
	char **p_av, **t_av, *ttype;

	/* If loaded at 0 and jumping through a NULL pointer, stop. */
	if (reenter++)
		abort();

	/* Create and initialize the global structure. */
	__global_list = gp = gs_init(argv[0]);
	clp = GCLP(gp);

#ifdef NOT_NEEDED_FOR_CURSES
	/*
	 * Strip out any arguments that vi isn't going to understand.  There's
	 * no way to portably call getopt twice, so arguments parsed here must
	 * be removed from the argument list.
	 */
	for (p_av = t_av = argv;;) {
		if (*t_av == NULL) {
			*p_av = NULL;
			break;
		}
		*p_av++ = *t_av++;
	}
#endif

	/*
	 * Initialize the terminal information.
	 *
	 * We have to know what terminal it is from the start, since we may
	 * have to use termcap/terminfo to find out how big the screen is.
	 */
	if ((ttype = getenv("TERM")) == NULL)
		ttype = "unknown";
	term_init(gp->progname, ttype);

	/* Add the terminal type to the global structure. */
	if ((OG_D_STR(gp, GO_TERM) =
	    OG_STR(gp, GO_TERM) = strdup(ttype)) == NULL)
		nomem(gp->progname);

	/* Figure out how big the screen is. */
	if (cl_ssize(NULL, 0, &rows, &cols, NULL))
		exit (1);

	/* Add the rows and columns to the global structure. */
	OG_VAL(gp, GO_LINES) = OG_D_VAL(gp, GO_LINES) = rows;
	OG_VAL(gp, GO_COLUMNS) = OG_D_VAL(gp, GO_COLUMNS) = cols;

	/* Ex wants stdout to be buffered. */
	(void)setvbuf(stdout, NULL, _IOFBF, 0);

	/* Start catching signals. */
	if (sig_init(gp, NULL))
		exit (1);

	/* Run ex/vi. */
	rval = editor(gp, argc, argv);

	/* Clean up signals. */
	sig_end(gp);

	/* Clean up the terminal. */
	(void)cl_quit(gp);

	/*
	 * XXX
	 * Reset the O_MESG option.
	 */
	if (clp->tgw != TGW_UNKNOWN)
		(void)cl_omesg(NULL, clp, clp->tgw == TGW_SET);

	/* If a killer signal arrived, pretend we just got it. */
	if (clp->killersig) {
		(void)signal(clp->killersig, SIG_DFL);
		(void)kill(getpid(), clp->killersig);
		/* NOTREACHED */
	}

	/* Free the global and CL private areas. */
#if defined(DEBUG) || defined(PURIFY) || defined(LIBRARY)
	free(clp);
	free(gp);
#endif

	exit (rval);
}

/*
 * gs_init --
 *	Create and partially initialize the GS structure.
 */
static GS *
gs_init(name)
	char *name;
{
	CL_PRIVATE *clp;
	GS *gp;
	int fd;
	char *p;

	/* Figure out what our name is. */
	if ((p = strrchr(name, '/')) != NULL)
		name = p + 1;

	/* Allocate the global structure. */
	CALLOC_NOMSG(NULL, gp, GS *, 1, sizeof(GS));

	/* Allocate the CL private structure. */
	if (gp != NULL)
		CALLOC_NOMSG(NULL, clp, CL_PRIVATE *, 1, sizeof(CL_PRIVATE));
	if (gp == NULL || clp == NULL)
		nomem(name);
	gp->cl_private = clp;

	/* Initialize the list of curses functions. */
	gp->scr_addstr = cl_addstr;
	gp->scr_attr = cl_attr;
	gp->scr_baud = cl_baud;
	gp->scr_bell = cl_bell;
	gp->scr_busy = NULL;
	gp->scr_clrtoeol = cl_clrtoeol;
	gp->scr_cursor = cl_cursor;
	gp->scr_deleteln = cl_deleteln;
	gp->scr_event = cl_event;
	gp->scr_ex_adjust = cl_ex_adjust;
	gp->scr_fmap = cl_fmap;
	gp->scr_insertln = cl_insertln;
	gp->scr_keyval = cl_keyval;
	gp->scr_move = cl_move;
	gp->scr_msg = NULL;
	gp->scr_optchange = cl_optchange;
	gp->scr_refresh = cl_refresh;
	gp->scr_rename = cl_rename;
	gp->scr_screen = cl_screen;
	gp->scr_suspend = cl_suspend;
	gp->scr_usage = cl_usage;

	/*
	 * Set the G_STDIN_TTY flag.  It's purpose is to avoid setting and
	 * resetting the tty if the input isn't from there.
	 */
	if (isatty(STDIN_FILENO))
		F_SET(gp, G_STDIN_TTY);

	/*
	 * We expect that if we've lost our controlling terminal that the
	 * open() (but not the tcgetattr()) will fail.
	 */
	if (F_ISSET(gp, G_STDIN_TTY)) {
		if (tcgetattr(STDIN_FILENO, &clp->orig) == -1)
			goto tcfail;
	} else if ((fd = open(_PATH_TTY, O_RDONLY, 0)) != -1) {
		if (tcgetattr(fd, &clp->orig) == -1) {
tcfail:			(void)fprintf(stderr,
			    "%s: tcgetattr: %s\n", name, strerror(errno));
			exit (1);
		}
		(void)close(fd);
	}

	gp->progname = name;
	return (gp);
}

/*
 * term_init --
 *	Initialize terminal information.
 */
static void
term_init(name, ttype)
	char *name, *ttype;
{
	int err;

	/* Set up the terminal database information. */
	setupterm(ttype, STDOUT_FILENO, &err);
	switch (err) {
	case -1:
		(void)fprintf(stderr,
		    "%s: No terminal database found\n", name);
		exit (1);
	case 0:
		(void)fprintf(stderr,
		    "%s: %s: unknown terminal type\n", name, ttype);
		exit (1);
	}
}

#define	GLOBAL_CLP \
	CL_PRIVATE *clp = GCLP(__global_list);
static void
h_hup(signo)
	int signo;
{
	GLOBAL_CLP;

	F_SET(clp, CL_SIGHUP);
	clp->killersig = SIGHUP;
}

static void
h_int(signo)
	int signo;
{
	GLOBAL_CLP;

	F_SET(clp, CL_SIGINT);
}

static void
h_term(signo)
	int signo;
{
	GLOBAL_CLP;

	F_SET(clp, CL_SIGTERM);
	clp->killersig = SIGTERM;
}

static void
h_winch(signo)
	int signo;
{
	GLOBAL_CLP;

	F_SET(clp, CL_SIGWINCH);
}
#undef	GLOBAL_CLP

/*
 * sig_init --
 *	Initialize signals.
 *
 * PUBLIC: int sig_init __P((GS *, SCR *));
 */
int
sig_init(gp, sp)
	GS *gp;
	SCR *sp;
{
	CL_PRIVATE *clp;

	clp = GCLP(gp);

	if (sp == NULL) {
		(void)sigemptyset(&__sigblockset);
		if (sigaddset(&__sigblockset, SIGHUP) ||
		    setsig(SIGHUP, &clp->oact[INDX_HUP], h_hup) ||
		    sigaddset(&__sigblockset, SIGINT) ||
		    setsig(SIGINT, &clp->oact[INDX_INT], h_int) ||
		    sigaddset(&__sigblockset, SIGTERM) ||
		    setsig(SIGTERM, &clp->oact[INDX_TERM], h_term) ||
		    sigaddset(&__sigblockset, SIGWINCH) ||
		    setsig(SIGWINCH, &clp->oact[INDX_WINCH], h_winch)) {
			(void)fprintf(stderr,
			    "%s: %s\n", gp->progname, strerror(errno));
			return (1);
		}
	} else
		if (setsig(SIGHUP, NULL, h_hup) ||
		    setsig(SIGINT, NULL, h_int) ||
		    setsig(SIGTERM, NULL, h_term) ||
		    setsig(SIGWINCH, NULL, h_winch)) {
			msgq(sp, M_SYSERR, "signal-reset");
		}
	return (0);
}

/*
 * setsig --
 *	Set a signal handler.
 */
static int
setsig(signo, oactp, handler)
	int signo;
	struct sigaction *oactp;
	void (*handler) __P((int));
{
	struct sigaction act;

	/*
	 * Use sigaction(2), not signal(3), since we don't always want to
	 * restart system calls.  The example is when waiting for a command
	 * mode keystroke and SIGWINCH arrives.  Besides, you can't portably
	 * restart system calls (thanks, POSIX!).  On the other hand, you
	 * can't portably NOT restart system calls (thanks, Sun!).  SunOS
	 * used SA_INTERRUPT as their extension to NOT restart read calls.
	 * We sure hope nobody else used it for anything else.  Mom told me
	 * there'd be days like this.  She just never told me that there'd
	 * be so many.
	 */
	act.sa_handler = handler;
	sigemptyset(&act.sa_mask);

#ifdef SA_INTERRUPT
	act.sa_flags = SA_INTERRUPT;
#else
	act.sa_flags = 0;
#endif
	return (sigaction(signo, &act, oactp));
}

/*
 * sig_end --
 *	End signal setup.
 */
static void
sig_end(gp)
	GS *gp;
{
	CL_PRIVATE *clp;

	clp = GCLP(gp);
	(void)sigaction(SIGHUP, NULL, &clp->oact[INDX_HUP]);
	(void)sigaction(SIGINT, NULL, &clp->oact[INDX_INT]);
	(void)sigaction(SIGTERM, NULL, &clp->oact[INDX_TERM]);
	(void)sigaction(SIGWINCH, NULL, &clp->oact[INDX_WINCH]);
}

/*
 * nomem --
 *	No memory error.
 */
static void
nomem(name)
	char *name;
{
	(void)fprintf(stderr, "%s: %s\n", name, strerror(errno));
	exit(1);
}
