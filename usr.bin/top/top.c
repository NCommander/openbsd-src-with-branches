/*	$OpenBSD: top.c,v 1.20 2003/06/13 21:52:25 deraadt Exp $	*/

/*
 *  Top users/processes display for Unix
 *  Version 3
 *
 * Copyright (c) 1984, 1989, William LeFebvre, Rice University
 * Copyright (c) 1989, 1990, 1992, William LeFebvre, Northwestern University
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS EMPLOYER BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

const char      copyright[] = "Copyright (c) 1984 through 1996, William LeFebvre";

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

/* includes specific to top */
#include "display.h"		/* interface to display package */
#include "screen.h"		/* interface to screen package */
#include "top.h"
#include "top.local.h"
#include "boolean.h"
#include "machine.h"
#include "utils.h"

/* Size of the stdio buffer given to stdout */
#define BUFFERSIZE	2048

/* The buffer that stdio will use */
char            stdoutbuf[BUFFERSIZE];

extern int      overstrike;

/* signal handling routines */
static void     leave(int);
static void     onalrm(int);
static void     tstop(int);
#ifdef SIGWINCH
static void     winch(int);
#endif

volatile sig_atomic_t leaveflag;
volatile sig_atomic_t tstopflag;
volatile sig_atomic_t winchflag;

static void     reset_display(void);
int		rundisplay(void);

/* values which need to be accessed by signal handlers */
static int      max_topn;	/* maximum displayable processes */

/* miscellaneous things */
jmp_buf         jmp_int;

/* routines that don't return int */

extern char *__progname;

extern int      (*proc_compares[])(const void *, const void *);
int order_index;

/* pointers to display routines */
void            (*d_loadave) () = i_loadave;
void            (*d_procstates) () = i_procstates;
void            (*d_cpustates) () = i_cpustates;
void            (*d_memory) () = i_memory;
void            (*d_message) () = i_message;
void            (*d_header) () = i_header;
void            (*d_process) () = i_process;

int displays = 0;	/* indicates unspecified */
char do_unames = Yes;
struct process_select ps;
char dostates = No;
char interactive = Maybe;
char warnings = 0;
double delay = Default_DELAY;
char *order_name = NULL;
int topn = Default_TOPN;
int no_command = 1;

#if Default_TOPN == Infinity
char topn_specified = No;
#endif

/*
 * these defines enumerate the "strchr"s of the commands in
 * command_chars
 */
#define CMD_redraw	0
#define CMD_update	1
#define CMD_quit	2
#define CMD_help1	3
#define CMD_help2	4
#define CMD_OSLIMIT	4	/* terminals with OS can only handle commands */
#define CMD_errors	5	/* less than or equal to CMD_OSLIMIT	   */
#define CMD_number1	6
#define CMD_number2	7
#define CMD_delay	8
#define CMD_displays	9
#define CMD_kill	10
#define CMD_renice	11
#define CMD_idletog     12
#define CMD_idletog2    13
#define CMD_user	14
#define CMD_system	15
#define CMD_order       16

void
usage(void)
{
	fprintf(stderr,
	    "Top version %s\n"
	    "Usage: %s [-ISbinqu] [-d x] [-s x] [-o field] [-U username] [number]\n",
	    version_string(), __progname);
}

void
parseargs(int ac, char **av)
{
	char *endp;
	int i;

	while ((i = getopt(ac, av, "SIbinqus:d:U:o:")) != -1) {
		switch (i) {
		case 'u':	/* toggle uid/username display */
			do_unames = !do_unames;
			break;

		case 'U':	/* display only username's processes */
			if ((ps.uid = userid(optarg)) == (uid_t)-1) {
				fprintf(stderr, "%s: unknown user\n", optarg);
				exit(1);
			}
			break;

		case 'S':	/* show system processes */
			ps.system = !ps.system;
			break;

		case 'I':	/* show idle processes */
			ps.idle = !ps.idle;
			break;

		case 'i':	/* go interactive regardless */
			interactive = Yes;
			break;

		case 'n':	/* batch, or non-interactive */
		case 'b':
			interactive = No;
			break;

		case 'd':	/* number of displays to show */
			if ((i = atoiwi(optarg)) != Invalid && i != 0) {
				displays = i;
				break;
			}				
			fprintf(stderr,
			    "%s: warning: display count should be positive "
			    "-- option ignored\n",
			    __progname);
			warnings++;
			break;

		case 's':
			delay = strtod(optarg, &endp);

			if (delay > 0 && delay <= 1000000 && *endp == '\0')
				break;

			fprintf(stderr,
			    "%s: warning: delay should be a non-negative number"
			    " -- using default\n",
			    __progname);
			delay = Default_DELAY;
			warnings++;
			break;

		case 'q':	/* be quick about it */
			/* only allow this if user is really root */
			if (getuid() == 0) {
				/* be very un-nice! */
				(void) nice(-20);
				break;
			}
			fprintf(stderr,
			    "%s: warning: `-q' option can only be used by root\n",
			    __progname);
			warnings++;
			break;

		case 'o':	/* select sort order */
			order_name = optarg;
			break;

		default:
			usage();
			exit(1);
		}
	}

	/* get count of top processes to display (if any) */
	if (optind < ac) {
		if ((topn = atoiwi(av[optind])) == Invalid) {
			fprintf(stderr,
			    "%s: warning: process display count should "
			    "be non-negative -- using default\n",
			    __progname);
			warnings++;
		}
#if Default_TOPN == Infinity
		else
			topn_specified = Yes;
#endif
	}
}

struct system_info system_info;
struct statics  statics;

int
main(int argc, char *argv[])
{
	char *uname_field = "USERNAME", *header_text, *env_top;
	char *(*get_userid)() = username, **preset_argv, **av;
	int preset_argc = 0, ac, active_procs, i;
	sigset_t mask, oldmask;
	time_t curr_time;
	caddr_t processes;

	/* set the buffer for stdout */
#ifdef DEBUG
	setbuffer(stdout, NULL, 0);
#else
	setbuffer(stdout, stdoutbuf, sizeof stdoutbuf);
#endif

	/* initialize some selection options */
	ps.idle = Yes;
	ps.system = No;
	ps.uid = (uid_t)-1;
	ps.command = NULL;

	/* get preset options from the environment */
	if ((env_top = getenv("TOP")) != NULL) {
		av = preset_argv = argparse(env_top, &preset_argc);
		ac = preset_argc;

		/*
		 * set the dummy argument to an explanatory message, in case
		 * getopt encounters a bad argument
		 */
		preset_argv[0] = "while processing environment";
	}
	/* process options */
	do {
		/*
		 * if we're done doing the presets, then process the real
		 * arguments
		 */
		if (preset_argc == 0) {
			ac = argc;
			av = argv;
			optind = 1;
		}
		parseargs(ac, av);
		i = preset_argc;
		preset_argc = 0;
	} while (i != 0);

	/* set constants for username/uid display correctly */
	if (!do_unames) {
		uname_field = "   UID  ";
		get_userid = itoa7;
	}
	/* initialize the kernel memory interface */
	if (machine_init(&statics) == -1)
		exit(1);

	/* determine sorting order index, if necessary */
	if (order_name != NULL) {
		if ((order_index = string_index(order_name,
		    statics.order_names)) == -1) {
			char **pp;

			fprintf(stderr, "%s: '%s' is not a recognized sorting order.\n",
			    __progname, order_name);
			fprintf(stderr, "\tTry one of these:");
			pp = statics.order_names;
			while (*pp != NULL)
				fprintf(stderr, " %s", *pp++);
			fputc('\n', stderr);
			exit(1);
		}
	}

	/* initialize termcap */
	init_termcap(interactive);

	/* get the string to use for the process area header */
	header_text = format_header(uname_field);

	/* initialize display interface */
	if ((max_topn = display_init(&statics)) == -1) {
		fprintf(stderr, "%s: can't allocate sufficient memory\n", __progname);
		exit(4);
	}
	/* print warning if user requested more processes than we can display */
	if (topn > max_topn) {
		fprintf(stderr,
		    "%s: warning: this terminal can only display %d processes.\n",
		    __progname, max_topn);
		warnings++;
	}
	/* adjust for topn == Infinity */
	if (topn == Infinity) {
		/*
		 *  For smart terminals, infinity really means everything that can
		 *  be displayed, or Largest.
		 *  On dumb terminals, infinity means every process in the system!
		 *  We only really want to do that if it was explicitly specified.
		 *  This is always the case when "Default_TOPN != Infinity".  But if
		 *  topn wasn't explicitly specified and we are on a dumb terminal
		 *  and the default is Infinity, then (and only then) we use
		 *  "Nominal_TOPN" instead.
		 */
#if Default_TOPN == Infinity
		topn = smart_terminal ? Largest :
		    (topn_specified ? Largest : Nominal_TOPN);
#else
		topn = Largest;
#endif
	}
	/* set header display accordingly */
	display_header(topn > 0);

	/* determine interactive state */
	if (interactive == Maybe)
		interactive = smart_terminal;

	/* if # of displays not specified, fill it in */
	if (displays == 0)
		displays = smart_terminal ? Infinity : 1;

	/*
	 * block interrupt signals while setting up the screen and the
	 * handlers
	 */
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGQUIT);
	sigaddset(&mask, SIGTSTP);
	sigprocmask(SIG_BLOCK, &mask, &oldmask);
	init_screen();
	(void) signal(SIGINT, leave);
	(void) signal(SIGQUIT, leave);
	(void) signal(SIGTSTP, tstop);
#ifdef SIGWINCH
	(void) signal(SIGWINCH, winch);
#endif
	sigprocmask(SIG_SETMASK, &oldmask, NULL);
	if (warnings) {
		fputs("....", stderr);
		fflush(stderr);	/* why must I do this? */
		sleep((unsigned) (3 * warnings));
		fputc('\n', stderr);
	}
restart:

	/*
	 *  main loop -- repeat while display count is positive or while it
	 *		indicates infinity (by being -1)
	 */
	while ((displays == -1) || (displays-- > 0)) {
		/* get the current stats */
		get_system_info(&system_info);

		/* get the current set of processes */
		processes = get_process_info(&system_info, &ps,
		    proc_compares[order_index]);

		/* display the load averages */
		(*d_loadave)(system_info.last_pid, system_info.load_avg);

		/* display the current time */
		/* this method of getting the time SHOULD be fairly portable */
		time(&curr_time);
		i_timeofday(&curr_time);

		/* display process state breakdown */
		(*d_procstates)(system_info.p_total, system_info.procstates);

		/* display the cpu state percentage breakdown */
		if (dostates) {	/* but not the first time */
			(*d_cpustates) (system_info.cpustates);
		} else {
			/* we'll do it next time */
			if (smart_terminal)
				z_cpustates();
			else {
				if (putchar('\n') == EOF)
					exit(1);
			}
			dostates = Yes;
		}

		/* display memory stats */
		(*d_memory) (system_info.memory);

		/* handle message area */
		(*d_message) ();

		/* update the header area */
		(*d_header) (header_text);

		if (topn > 0) {
			/* determine number of processes to actually display */
			/*
			 * this number will be the smallest of:  active
			 * processes, number user requested, number current
			 * screen accommodates
			 */
			active_procs = system_info.p_active;
			if (active_procs > topn)
				active_procs = topn;
			if (active_procs > max_topn)
				active_procs = max_topn;
			/* now show the top "n" processes. */
			for (i = 0; i < active_procs; i++)
				(*d_process)(i, format_next_process(processes,
				    get_userid));
		} else
			i = 0;

		/* do end-screen processing */
		u_endscreen(i);

		/* now, flush the output buffer */
		fflush(stdout);

		/* only do the rest if we have more displays to show */
		if (displays) {
			/* switch out for new display on smart terminals */
			if (smart_terminal) {
				if (overstrike) {
					reset_display();
				} else {
					d_loadave = u_loadave;
					d_procstates = u_procstates;
					d_cpustates = u_cpustates;
					d_memory = u_memory;
					d_message = u_message;
					d_header = u_header;
					d_process = u_process;
				}
			}
			no_command = Yes;
			if (!interactive) {
				/* set up alarm */
				(void) signal(SIGALRM, onalrm);
				(void) alarm((unsigned) delay);

				/* wait for the rest of it .... */
				pause();
			} else {
				while (no_command)
					if (rundisplay())
						goto restart;
			}
		}
	}

	quit(0);
	/* NOTREACHED */
	return (0);
}

int
rundisplay(void)
{
	static char tempbuf1[50], tempbuf2[50];
	struct timeval timeout;
	fd_set readfds;
	sigset_t mask;
	char ch, *iptr;
	int change, i;
	uid_t uid;
	static char command_chars[] = "\f qh?en#sdkriIuSo";

	/*
	 * assume valid command unless told
	 * otherwise
	 */
	no_command = No;

	/*
	 * set up arguments for select with
	 * timeout
	 */
	FD_ZERO(&readfds);
	FD_SET(STDIN_FILENO, &readfds);
	timeout.tv_sec = (long) delay;
	timeout.tv_usec = (long) ((delay - timeout.tv_sec) * 1000000);

	if (leaveflag) {
		end_screen();
		exit(0);
	}
	if (tstopflag) {
		/* move to the lower left */
		end_screen();
		fflush(stdout);

		/*
		 * default the signal handler
		 * action
		 */
		(void) signal(SIGTSTP, SIG_DFL);

		/*
		 * unblock the signal and
		 * send ourselves one
		 */
		sigemptyset(&mask);
		sigaddset(&mask, SIGTSTP);
		sigprocmask(SIG_UNBLOCK, &mask, NULL);
		(void) kill(0, SIGTSTP);

		/* reset the signal handler */
		(void) signal(SIGTSTP, tstop);

		/* reinit screen */
		reinit_screen();
		reset_display();
		tstopflag = 0;
		return 1;
	}
	if (winchflag) {
		/*
		 * reascertain the screen
		 * dimensions
		 */
		get_screensize();

		/* tell display to resize */
		max_topn = display_resize();

		/* reset the signal handler */
		(void) signal(SIGWINCH, winch);

		reset_display();
		winchflag = 0;
		return 1;
	}
	/*
	 * wait for either input or the end
	 * of the delay period
	 */
	if (select(STDIN_FILENO + 1, &readfds, (fd_set *) NULL,
	    (fd_set *) NULL, &timeout) > 0) {
		char *errmsg;
		int newval;

		clear_message();

		/*
		 * now read it and convert to
		 * command strchr
		 */
		(void) read(0, &ch, 1);
		if ((iptr = strchr(command_chars, ch)) == NULL) {
			/* illegal command */
			new_message(MT_standout, " Command not understood");
			if (putchar('\r') == EOF)
				exit(1);
			no_command = Yes;
			fflush(stdout);
			return (0);
		}

		change = iptr - command_chars;
		if (overstrike && change > CMD_OSLIMIT) {
			/* error */
			new_message(MT_standout,
			    " Command cannot be handled by this terminal");
			if (putchar('\r') == EOF)
				exit(1);
			no_command = Yes;
			fflush(stdout);
			return (0);
		}

		switch (change) {
		case CMD_redraw:	/* redraw screen */
			reset_display();
			break;

		case CMD_update:	/* merely update display */
			/*
			 * is the load average high?
			 */
			if (system_info.load_avg[0] > LoadMax) {
				/* yes, go home for visual feedback */
				go_home();
				fflush(stdout);
			}
			break;

		case CMD_quit:	/* quit */
			quit(0);
			break;

		case CMD_help1:	/* help */
		case CMD_help2:
			reset_display();
			clear();
			show_help();
			standout("Hit any key to continue: ");
			fflush(stdout);
			(void) read(0, &ch, 1);
			break;

		case CMD_errors:	/* show errors */
			if (error_count() == 0) {
				new_message(MT_standout,
				    " Currently no errors to report.");
				if (putchar('\r') == EOF)
					exit(1);
				no_command = Yes;
			} else {
				reset_display();
				clear();
				show_errors();
				standout("Hit any key to continue: ");
				fflush(stdout);
				(void) read(0, &ch, 1);
			}
			break;

		case CMD_number1:	/* new number */
		case CMD_number2:
			new_message(MT_standout,
			    "Number of processes to show: ");
			newval = readline(tempbuf1, 8, Yes);
			if (newval > -1) {
				if (newval > max_topn) {
					new_message(MT_standout | MT_delayed,
					    " This terminal can only "
					    "display %d processes.",
					    max_topn);
					if (putchar('\r') == EOF)
						exit(1);
				}
				if (newval == 0)
					display_header(No);
				else if (newval > topn && topn == 0) {
					/* redraw the header */
					display_header(Yes);
					d_header = i_header;
				}
				topn = newval;
			}
			break;

		case CMD_delay:	/* new seconds delay */
			new_message(MT_standout, "Seconds to delay: ");
			if (readline(tempbuf2, sizeof(tempbuf2), No) > 0) {
				char *endp;
				double newdelay = strtod(tempbuf2, &endp);

				if (newdelay >= 0 && newdelay < 1000000 &&
				    *endp == '\0')
					delay = newdelay;
			}
			clear_message();
			break;

		case CMD_displays:	/* change display count */
			new_message(MT_standout,
			    "Displays to show (currently %s): ",
			    displays == -1 ? "infinite" :
			    itoa(displays));
			if ((i = readline(tempbuf1, 10, Yes)) > 0)
				displays = i;
			else if (i == 0)
				quit(0);

			clear_message();
			break;

		case CMD_kill:	/* kill program */
			new_message(0, "kill ");
			if (readline(tempbuf2, sizeof(tempbuf2), No) > 0) {
				if ((errmsg = kill_procs(tempbuf2)) != NULL) {
					new_message(MT_standout, "%s", errmsg);
					if (putchar('\r') == EOF)
						exit(1);
					no_command = Yes;
				}
			} else
				clear_message();
			break;

		case CMD_renice:	/* renice program */
			new_message(0, "renice ");
			if (readline(tempbuf2, sizeof(tempbuf2), No) > 0) {
				if ((errmsg = renice_procs(tempbuf2)) != NULL) {
					new_message(MT_standout, "%s", errmsg);
					if (putchar('\r') == EOF)
						exit(1);
					no_command = Yes;
				}
			} else
				clear_message();
			break;

		case CMD_idletog:
		case CMD_idletog2:
			ps.idle = !ps.idle;
			new_message(MT_standout | MT_delayed,
			    " %sisplaying idle processes.",
			    ps.idle ? "D" : "Not d");
			if (putchar('\r') == EOF)
				exit(1);
			break;

		case CMD_user:
			new_message(MT_standout,
			    "Username to show: ");
			if (readline(tempbuf2, sizeof(tempbuf2), No) > 0) {
				if (tempbuf2[0] == '+' &&
				    tempbuf2[1] == '\0') {
					ps.uid = (uid_t)-1;
				} else if ((uid = userid(tempbuf2)) == (uid_t)-1) {
					new_message(MT_standout,
					    " %s: unknown user", tempbuf2);
					no_command = Yes;
				} else
					ps.uid = uid;
				if (putchar('\r') == EOF)
					exit(1);
			} else
				clear_message();
			break;

		case CMD_system:
			ps.system = !ps.system;
			new_message(MT_standout | MT_delayed,
			    " %sisplaying system processes.",
			    ps.system ? "D" : "Not d");
			break;

		case CMD_order:
			new_message(MT_standout,
			    "Order to sort: ");
			if (readline(tempbuf2,
			    sizeof(tempbuf2), No) > 0) {
				if ((i = string_index(tempbuf2,
				    statics.order_names)) == -1) {
					new_message(MT_standout,
					    " %s: unrecognized sorting order",
					    tempbuf2);
					no_command = Yes;
				} else
					order_index = i;
				if (putchar('\r') == EOF)
					exit(1);
			} else
				clear_message();
			break;

		default:
			new_message(MT_standout, " BAD CASE IN SWITCH!");
			if (putchar('\r') == EOF)
				exit(1);
		}
	}

	/* flush out stuff that may have been written */
	fflush(stdout);
	return 0;
}


/*
 *  reset_display() - reset all the display routine pointers so that entire
 *	screen will get redrawn.
 */
static void
reset_display(void)
{
	d_loadave = i_loadave;
	d_procstates = i_procstates;
	d_cpustates = i_cpustates;
	d_memory = i_memory;
	d_message = i_message;
	d_header = i_header;
	d_process = i_process;
}

void
leave(int signo)
{
	leaveflag = 1;
}

void
tstop(int signo)
{
	tstopflag = 1;
}

#ifdef SIGWINCH
void
winch(int signo)
{
	winchflag = 1;
}
#endif

void
onalrm(int signo)
{
}

void
quit(int ret)
{
	end_screen();
	exit(ret);
}
