/*	$OpenBSD: main.c,v 1.34 2019/06/28 13:35:02 deraadt Exp $	*/
/*	$NetBSD: main.c,v 1.7 1997/05/13 06:15:57 mikel Exp $	*/

/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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

#include "rcv.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include "extern.h"

int	msgCount;			/* Count of messages read in */
int	rcvmode;			/* True if receiving mail */
int	sawcom;				/* Set after first command */
int	senderr;			/* An error while checking */
int	edit;				/* Indicates editing a file */
int	readonly;			/* Will be unable to rewrite file */
int	noreset;			/* String resets suspended */
int	sourcing;			/* Currently reading variant file */
int	loading;			/* Loading user definitions */
int	cond;				/* Current state of conditional exc. */
FILE	*itf;				/* Input temp file buffer */
FILE	*otf;				/* Output temp file buffer */
int	image;				/* File descriptor for image of msg */
FILE	*input;				/* Current command input file */
char	mailname[PATHSIZE];		/* Name of current file */
char	prevfile[PATHSIZE];		/* Name of previous file */
char	*homedir;			/* Path name of home directory */
const char
	*myname;			/* My login name */
off_t	mailsize;			/* Size of system mailbox */
int	lexnumber;			/* Number of TNUMBER from scan() */
char	lexstring[STRINGLEN];		/* String from TSTRING, scan() */
int	regretp;			/* Pointer to TOS of regret tokens */
int	regretstack[REGDEP];		/* Stack of regretted tokens */
char	*string_stack[REGDEP];		/* Stack of regretted strings */
int	numberstack[REGDEP];		/* Stack of regretted numbers */
struct	message	*dot;			/* Pointer to current message */
struct	message	*message;		/* The actual message structure */
struct	var	*variables[HSHSIZE];	/* Pointer to active var list */
struct	grouphead	*groups[HSHSIZE];/* Pointer to active groups */
struct	ignoretab	ignore[2];	/* ignored and retained fields
					   0 is ignore, 1 is retain */
struct	ignoretab	saveignore[2];	/* ignored and retained fields
					   on save to folder */
struct	ignoretab	ignoreall[2];	/* special, ignore all headers */
char	**altnames;			/* List of alternate names for user */
int	debug;				/* Debug flag set */
int	screenwidth;			/* Screen width, or best guess */
int	screenheight;			/* Screen height, or best guess,
					   for "header" command */
int	realscreenheight;		/* the real screen height */
int	uflag;				/* Are we in -u mode? */
sigset_t intset;			/* Signal set that is just SIGINT */

/*
 * The pointers for the string allocation routines,
 * there are NSPACE independent areas.
 * The first holds STRINGSIZE bytes, the next
 * twice as much, and so on.
 */
struct strings stringdope[NSPACE];

__dead	void	usage(void);
	int	main(int, char **);

/*
 * Mail -- a mail program
 *
 * Startup -- interface with user.
 */

int
main(int argc, char **argv)
{
	int i;
	struct name *to, *cc, *bcc, *smopts;
	char *fromaddr;
	char *subject;
	char *ef;
	char nosrc = 0;
	char *rc;
	extern const char version[];

	if (pledge("stdio rpath wpath cpath getpw tmppath fattr tty flock proc exec",
	    NULL) == -1)
		err(1, "pledge");

	/*
	 * Set up a reasonable environment.
	 * Figure out whether we are being run interactively,
	 * start the SIGCHLD catcher, and so forth.
	 */
	(void)signal(SIGCHLD, sigchild);
	(void)signal(SIGPIPE, SIG_IGN);
	if (isatty(0))
		assign("interactive", "");
	image = -1;
	/*
	 * Now, determine how we are being used.
	 * We successively pick off - flags.
	 * If there is anything left, it is the base of the list
	 * of users to mail to.  Argp will be set to point to the
	 * first of these users.
	 */
	ef = NULL;
	to = NULL;
	cc = NULL;
	bcc = NULL;
	smopts = NULL;
	fromaddr = NULL;
	subject = NULL;
	while ((i = getopt(argc, argv, "EINb:c:dfinr:s:u:v")) != -1) {
		switch (i) {
		case 'u':
			/*
			 * Next argument is person to pretend to be.
			 */
			if (strlen(optarg) >= LOGIN_NAME_MAX)
				errx(1, "username `%s' too long", optarg);
			unsetenv("MAIL");
			myname = optarg;
			uflag = 1;
			break;
		case 'i':
			/*
			 * User wants to ignore interrupts.
			 * Set the variable "ignore"
			 */
			assign("ignore", "");
			break;
		case 'd':
			debug++;
			break;
		case 'r':
			/*
			 * Set From: address
			 */
			fromaddr = optarg;
			break;
		case 's':
			/*
			 * Give a subject field for sending from
			 * non terminal
			 */
			subject = optarg;
			break;
		case 'f':
			/*
			 * User is specifying file to "edit" with Mail,
			 * as opposed to reading system mailbox.
			 * We read his mbox file unless another file
			 * is specified after the arguments.
			 */
			ef = "&";
			break;
		case 'n':
			/*
			 * User doesn't want to source /usr/lib/Mail.rc
			 */
			nosrc = 1;
			break;
		case 'N':
			/*
			 * Avoid initial header printing.
			 */
			assign("noheader", "");
			break;
		case 'v':
			/*
			 * Send mailer verbose flag
			 */
			assign("verbose", "");
			break;
		case 'I':
			/*
			 * We're interactive
			 */
			assign("interactive", "");
			break;
		case 'c':
			/*
			 * Get Carbon Copy Recipient list
			 */
			cc = cat(cc, nalloc(optarg, GCC));
			break;
		case 'b':
			/*
			 * Get Blind Carbon Copy Recipient list
			 */
			bcc = cat(bcc, nalloc(optarg, GBCC));
			break;
		case 'E':
			/*
			 * Don't send messages with an empty body.
			 */
			assign("skipempty", "");
			break;
		default:
			usage();
			/*NOTREACHED*/
		}
	}
	if (ef != NULL) {
		/* Check for optional mailbox file name. */
		if (optind < argc) {
			ef = argv[optind++];
			if (optind < argc)
			    errx(1, "Cannot give -f and people to send to");
		}
	} else {
		for (i = optind; argv[i]; i++)
			to = cat(to, nalloc(argv[i], GTO));
	}
	/*
	 * Check for inconsistent arguments.
	 */
	if (to == NULL && (subject != NULL || cc != NULL || bcc != NULL ||
	    fromaddr != NULL))
		errx(1, "You must specify direct recipients with -s, -c, -b, "
		    "or -r");
	/*
	 * Block SIGINT except where we install an explicit handler for it.
	 */
	sigemptyset(&intset);
	sigaddset(&intset, SIGINT);
	(void)sigprocmask(SIG_BLOCK, &intset, NULL);
	/*
	 * Initialization.
	 */
	tinit();
	setscreensize();
	input = stdin;
	rcvmode = !to;
	spreserve();
	if (!nosrc)
		load(_PATH_MASTER_RC);
	/*
	 * Expand returns a savestr, but load only uses the file name
	 * for fopen, so it's safe to do this.
	 */
	if ((rc = getenv("MAILRC")) == 0)
		rc = "~/.mailrc";
	load(expand(rc));
	if (!rcvmode) {
		mail(to, cc, bcc, smopts, fromaddr, subject);
		/*
		 * why wait?
		 */
		exit(senderr);
	}
	/*
	 * Ok, we are reading mail.
	 * Decide whether we are editing a mailbox or reading
	 * the system mailbox, and open up the right stuff.
	 */
	if (ef == NULL)
		ef = "%";
	if (setfile(ef) < 0)
		exit(1);		/* error already reported */

	if (value("quiet") == NULL)
		(void)printf("Mail version %s.  Type ? for help.\n",
			version);
	announce();
	(void)fflush(stdout);
	commands();
	(void)ignoresig(SIGHUP, NULL, NULL);
	(void)ignoresig(SIGINT, NULL, NULL);
	(void)ignoresig(SIGQUIT, NULL, NULL);
	quit();
	exit(0);
}

/*
 * Compute what the screen size for printing headers should be.
 * We use the following algorithm for the height:
 *	If baud rate < 1200, use  9
 *	If baud rate = 1200, use 14
 *	If baud rate > 1200, use 24 or ws_row
 * Width is either 80 or ws_col;
 */
void
setscreensize(void)
{
	struct termios tbuf;
	struct winsize ws;
	speed_t ospeed;

	if (ioctl(1, TIOCGWINSZ, (char *) &ws) == -1)
		ws.ws_col = ws.ws_row = 0;
	if (tcgetattr(1, &tbuf) == -1)
		ospeed = 9600;
	else
		ospeed = cfgetospeed(&tbuf);
	if (ospeed < B1200)
		screenheight = 9;
	else if (ospeed == B1200)
		screenheight = 14;
	else if (ws.ws_row != 0)
		screenheight = ws.ws_row;
	else
		screenheight = 24;
	if ((realscreenheight = ws.ws_row) == 0)
		realscreenheight = 24;
	if ((screenwidth = ws.ws_col) == 0)
		screenwidth = 80;
}

__dead void
usage(void)
{

	fprintf(stderr, "usage: %s [-dEIinv] [-b list] [-c list] "
	    "[-r from-addr] [-s subject] to-addr ...\n", __progname);
	fprintf(stderr, "       %s [-dEIiNnv] -f [file]\n", __progname);
	fprintf(stderr, "       %s [-dEIiNnv] [-u user]\n", __progname);
	exit(1);
}
