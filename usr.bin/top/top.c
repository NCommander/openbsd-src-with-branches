/*	$OpenBSD: top.c,v 1.46 2007/02/04 15:01:11 otto Exp $	*/

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

const char	copyright[] = "Copyright (c) 1984 through 1996, William LeFebvre";

#include <sys/types.h>
#include <sys/time.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <poll.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>

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
char		stdoutbuf[BUFFERSIZE];

extern int	overstrike;

/* signal handling routines */
static void	leave(int);
static void	onalrm(int);
static void	tstop(int);
static void	winch(int);

volatile sig_atomic_t leaveflag, tstopflag, winchflag;

static void	reset_display(void);
int		rundisplay(void);

static int	max_topn;	/* maximum displayable processes */

extern int	(*proc_compares[])(const void *, const void *);
int order_index;

/* pointers to display routines */
void		(*d_loadave)(int, double *) = i_loadave;
void		(*d_procstates)(int, int *) = i_procstates;
void		(*d_cpustates)(int64_t *) = i_cpustates;
void		(*d_memory)(int *) = i_memory;
void		(*d_message)(void) = i_message;
void		(*d_header)(char *) = i_header;
void		(*d_process)(int, char *) = i_process;

int displays = 0;	/* indicates unspecified */
char do_unames = Yes;
struct process_select ps;
char interactive = Maybe;
char warnings = 0;
double delay = Default_DELAY;
char *order_name = NULL;
int topn = Default_TOPN;
int no_command = Yes;
int old_system = No;
int old_threads = No;
int show_args = No;

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
#define CMD_idletog	12
#define CMD_idletog2	13
#define CMD_user	14
#define CMD_system	15
#define CMD_order	16
#define CMD_pid		17
#define CMD_command	18
#define CMD_threads	19
#define CMD_grep	20
#define CMD_add		21

static void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,
	    "usage: %s [-bCIinqSTu] [-d count] [-g command] [-o field] [-p pid] [-s time]\n\t[-U username] [number]\n",
	    __progname);
}

static void
parseargs(int ac, char **av)
{
	char *endp;
	int i;

	while ((i = getopt(ac, av, "STICbinqus:d:p:U:o:g:")) != -1) {
		switch (i) {
		case 'C':
			show_args = Yes;
			break;

		case 'u':	/* toggle uid/username display */
			do_unames = !do_unames;
			break;

		case 'U':	/* display only username's processes */
			if ((ps.uid = userid(optarg)) == (uid_t)-1) {
				fprintf(stderr, "%s: unknown user\n", optarg);
				exit(1);
			}
			break;

		case 'p': {	/* display only process id */
			unsigned long long num;
			const char *errstr;

			num = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL || !find_pid(num)) {
				fprintf(stderr, "%s: unknown pid\n", optarg);
				exit(1);
			}
			ps.pid = (pid_t)num;
			ps.system = Yes;
			break;
		}

		case 'S':	/* show system processes */
			ps.system = Yes;
			old_system = Yes;
			break;

		case 'T':	/* show threads */
			ps.threads = Yes;
			old_threads = Yes;
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
			warnx("warning: display count should be positive "
			    "-- option ignored");
			warnings++;
			break;

		case 's':
			delay = strtod(optarg, &endp);

			if (delay > 0 && delay <= 1000000 && *endp == '\0')
				break;

			warnx("warning: delay should be a non-negative number"
			    " -- using default");
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
			warnx("warning: `-q' option can only be used by root");
			warnings++;
			break;

		case 'o':	/* select sort order */
			order_name = optarg;
			break;

		case 'g':	/* grep command name */
			ps.command = strdup(optarg);
			break;

		default:
			usage();
			exit(1);
		}
	}

	/* get count of top processes to display (if any) */
	if (optind < ac) {
		if ((topn = atoiwi(av[optind])) == Invalid) {
			warnx("warning: process display count should "
			    "be non-negative -- using default");
			warnings++;
			topn = Infinity;
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
	char *(*get_userid)(uid_t) = username, **preset_argv, **av;
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
	ps.pid = (pid_t)-1;
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
		get_userid = format_uid;
	}
	/* initialize the kernel memory interface */
	if (machine_init(&statics) == -1)
		exit(1);

	/* determine sorting order index, if necessary */
	if (order_name != NULL) {
		if ((order_index = string_index(order_name,
		    statics.order_names)) == -1) {
			char **pp;

			warnx("'%s' is not a recognized sorting order",
			    order_name);
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
		warnx("can't allocate sufficient memory");
		exit(4);
	}
	/* print warning if user requested more processes than we can display */
	if (topn > max_topn) {
		warnx("warning: this terminal can only display %d processes",
		    max_topn);
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
	siginterrupt(SIGINT, 1);
	(void) signal(SIGQUIT, leave);
	(void) signal(SIGTSTP, tstop);
	(void) signal(SIGWINCH, winch);
	sigprocmask(SIG_SETMASK, &oldmask, NULL);
	if (warnings) {
		fputs("....", stderr);
		fflush(stderr);	/* why must I do this? */
		sleep((unsigned)(3 * warnings));
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
		(*d_cpustates)(system_info.cpustates);

		/* display memory stats */
		(*d_memory)(system_info.memory);

		/* handle message area */
		(*d_message)();

		/* update the header area */
		(*d_header)(header_text);

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
				if (leaveflag)
					exit(0);
				if (tstopflag) {
					(void) signal(SIGTSTP, SIG_DFL);
					(void) kill(0, SIGTSTP);
					/* reset the signal handler */
					(void) signal(SIGTSTP, tstop);
					tstopflag = 0;
				}
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
	sigset_t mask;
	char ch, *iptr;
	int change, i;
	struct pollfd pfd[1];
	uid_t uid;
	static char command_chars[] = "\f qh?en#sdkriIuSopCTg+";

	/*
	 * assume valid command unless told
	 * otherwise
	 */
	no_command = No;

	/*
	 * set up arguments for select with
	 * timeout
	 */
	pfd[0].fd = STDIN_FILENO;
	pfd[0].events = POLLIN;

	if (leaveflag)
		quit(0);
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
	if (poll(pfd, 1, (int)(delay * 1000)) > 0 &&
	    !(pfd[0].revents & (POLLERR|POLLHUP|POLLNVAL))) {
		char *errmsg;
		ssize_t len;
		int newval;

		clear_message();

		/*
		 * now read it and convert to
		 * command strchr
		 */
		while (1) {
			len = read(STDIN_FILENO, &ch, 1);
			if (len == -1 && errno == EINTR)
				continue;
			if (len == 0)
				exit(1);
			break;
		}
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
			while (1) {
				len = read(STDIN_FILENO, &ch, 1);
				if (len == -1 && errno == EINTR)
					continue;
				if (len == 0)
					exit(1);
				break;
			}
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
				while (1) {
					len = read(STDIN_FILENO, &ch, 1);
					if (len == -1 && errno == EINTR)
						continue;
					if (len == 0)
						exit(1);
					break;
				}
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
			old_system = ps.system;
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

		case CMD_pid:
			new_message(MT_standout, "Process ID to show: ");
			if (readline(tempbuf2, sizeof(tempbuf2), No) > 0) {
				if (tempbuf2[0] == '+' &&
				    tempbuf2[1] == '\0') {
					ps.pid = (pid_t)-1;
					ps.system = old_system;
				} else {
					unsigned long long num;
					const char *errstr;

					num = strtonum(tempbuf2, 0, INT_MAX,
					    &errstr);
					if (errstr != NULL || !find_pid(num)) {
						new_message(MT_standout,
						    " %s: unknown pid",
						    tempbuf2);
						no_command = Yes;
					} else {
						if (ps.system == No)
							old_system = No;
						ps.pid = (pid_t)num;
						ps.system = Yes;
					}
				}
				if (putchar('\r') == EOF)
					exit(1);
			} else
				clear_message();
			break;

		case CMD_command:
			show_args = (show_args == No) ? Yes : No;
			break;
		
		case CMD_threads:
			ps.threads = !ps.threads;
			old_threads = ps.threads;
			new_message(MT_standout | MT_delayed,
			    " %sisplaying threads.",
			    ps.threads ? "D" : "Not d");
			break;

		case CMD_grep:
			new_message(MT_standout,
			    "Grep command name: ");
			if (readline(tempbuf2, sizeof(tempbuf2), No) > 0) {
				free(ps.command);
				if (tempbuf2[0] == '+' &&
				    tempbuf2[1] == '\0')
					ps.command = NULL;
				else
					ps.command = strdup(tempbuf2);
				if (putchar('\r') == EOF)
					exit(1);
			} else
				clear_message();
			break;

		case CMD_add:
			ps.uid = (uid_t)-1;	/* uid */
			ps.pid = (pid_t)-1; 	/* pid */
			ps.system = old_system;
			ps.command = NULL;	/* grep */
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

/* ARGSUSED */
void
leave(int signo)
{
	leaveflag = 1;
}

/* ARGSUSED */
void
tstop(int signo)
{
	tstopflag = 1;
}

/* ARGSUSED */
void
winch(int signo)
{
	winchflag = 1;
}

/* ARGSUSED */
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
