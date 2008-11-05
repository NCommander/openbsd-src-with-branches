/* $Id: main.c,v 1.49 2008/11/04 19:00:08 espie Exp $	 */
/*
 * Copyright (c) 2001, 2007 Can Erkin Acar
 * Copyright (c) 2001 Daniel Hartmeier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>


#include <ctype.h>
#include <curses.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <utmp.h>

#include "engine.h"
#include "systat.h"

double	dellave;

kvm_t	*kd;
char	*nlistf = NULL;
char	*memf = NULL;
double	avenrun[3];
double	naptime = 5.0;
int	verbose = 1;		/* to report kvm read errs */
int	nflag = 1;
int	ut, hz, stathz;
char    hostname[MAXHOSTNAMELEN];
WINDOW  *wnd;
int	CMDLINE;

#define TIMEPOS 55

int  ucount(void);
void usage(void);

/* command prompt */

void cmd_delay(const char *);
void cmd_count(const char *);
void cmd_compat(const char *);

struct command cm_compat = {"Command", cmd_compat};
struct command cm_delay = {"Seconds to delay", cmd_delay};
struct command cm_count = {"Number of lines to display", cmd_count};


/* display functions */

int
print_header(void)
{
	time_t now;
	int start = dispstart + 1, end = dispstart + maxprint;
	char tbuf[26];

	if (end > num_disp)
		end = num_disp;

	tb_start();

	getloadavg(avenrun, sizeof(avenrun) / sizeof(avenrun[0]));

	time(&now);
	strlcpy(tbuf, ctime(&now), sizeof tbuf);
	tbprintf("   %d users", ucount());
	tbprintf("    Load %.2f %.2f %.2f", avenrun[0], avenrun[1], avenrun[2]);
	if (num_disp && (start > 1 || end != num_disp))
		tbprintf("  (%u-%u of %u)", start, end, num_disp);

	if (paused)
		tbprintf(" PAUSED");

	if (rawmode)
		printf("\n\n%s\n", tmp_buf);
	else
		mvprintw(0, 0, "%s", tmp_buf);

	mvprintw(0, TIMEPOS, "%s", tbuf);


	return (1);
}

/* compatibility functions, rearrange later */
void
error(const char *fmt, ...)
{
	va_list ap;
	char buf[MAX_LINE_BUF];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);

	message_set(buf);
}

void
nlisterr(struct nlist namelist[])
{
	int i, n;

	n = 0;
	clear();
	mvprintw(2, 10, "systat: nlist: can't find following symbols:");
	for (i = 0;
	    namelist[i].n_name != NULL && *namelist[i].n_name != '\0'; i++)
		if (namelist[i].n_value == 0)
			mvprintw(2 + ++n, 10, "%s", namelist[i].n_name);
	move(CMDLINE, 0);
	clrtoeol();
	refresh();
	endwin();
	exit(1);
}

void
die(void)
{
	if (!rawmode)
		endwin();
	exit(0);
}


int
prefix(char *s1, char *s2)
{

	while (*s1 == *s2) {
		if (*s1 == '\0')
			return (1);
		s1++, s2++;
	}
	return (*s1 == '\0');
}

/* calculate number of users on the system */
int
ucount(void)
{
	int nusers = 0;
	struct	utmp utmp;

	if (ut < 0)
		return (0);
	lseek(ut, (off_t)0, SEEK_SET);
	while (read(ut, &utmp, sizeof(utmp)))
		if (utmp.ut_name[0] != '\0')
			nusers++;

	return (nusers);
}

/* main program functions */

void
usage(void)
{
	extern char *__progname;
	fprintf(stderr, "usage: %s [-abin] [-d count] "
	    "[-s delay] [-w width] [view] [delay]\n", __progname);
	exit(1);
}

void
show_view(void)
{
	if (rawmode)
		return;

	tb_start();
	tbprintf("%s %g", curr_view->name, naptime);
	tb_end();
	message_set(tmp_buf);
}

void
add_view_tb(field_view *v)
{
	if (curr_view == v)
		tbprintf("[%s] ", v->name);
	else
		tbprintf("%s ", v->name);
}

void
show_help(void)
{
	if (rawmode)
		return;

	tb_start();
	foreach_view(add_view_tb);
	tb_end();
	message_set(tmp_buf);
}

void
cmd_compat(const char *buf)
{
	const char *s;

	if (strcasecmp(buf, "help") == 0) {
		show_help();
		need_update = 1;
		return;
	}
	if (strcasecmp(buf, "quit") == 0 || strcasecmp(buf, "q") == 0) {
		gotsig_close = 1;
		return;
	}
	if (strcasecmp(buf, "stop") == 0) {
		paused = 1;
		gotsig_alarm = 1;
		return;
	}
	if (strncasecmp(buf, "start", 5) == 0) {
		paused = 0;
		gotsig_alarm = 1;
		cmd_delay(buf + 5);
		return;
	}

	for (s = buf; *s && strchr("0123456789+-.eE", *s) != NULL; s++)
		;
	if (*s) {
		if (set_view(buf))
			error("Invalid/ambiguous view: %s", buf);
	} else
		cmd_delay(buf);
}

void
cmd_delay(const char *buf)
{
	double del;
	del = atof(buf);

	if (del > 0) {
		udelay = (useconds_t)(del * 1000000);
		gotsig_alarm = 1;
		naptime = del;
	}
}

void
cmd_count(const char *buf)
{
	int ms;
	ms = atoi(buf);

	if (ms <= 0 || ms > lines - HEADER_LINES)
		maxprint = lines - HEADER_LINES;
	else
		maxprint = ms;
}


int
keyboard_callback(int ch)
{
	switch (ch) {
	case '?':
		/* FALLTHROUGH */
	case 'h':
		show_help();
		need_update = 1;
		break;
	case CTRL_G:
		show_view();
		need_update = 1;
		break;
	case 'l':
		command_set(&cm_count, NULL);
		break;
	case 's':
		command_set(&cm_delay, NULL);
		break;
	case ':':
		command_set(&cm_compat, NULL);
		break;
	default:
		return 0;
	};

	return 1;
}

void
initialize(void)
{
	engine_initialize();

	initvmstat();
	initpigs();
	initifstat();
	initiostat();
	initsensors();
	initmembufs();
	initnetstat();
	initswap();
	initpftop();
	initpf();
	initpool();
}

void
gethz(void)
{
	struct clockinfo cinf;
	size_t  size = sizeof(cinf);
	int	mib[2];

	mib[0] = CTL_KERN;
	mib[1] = KERN_CLOCKRATE;
	if (sysctl(mib, 2, &cinf, &size, NULL, 0) == -1)
		return;
	stathz = cinf.stathz;
	hz = cinf.hz;
}

int
main(int argc, char *argv[])
{
	char errbuf[_POSIX2_LINE_MAX];
	extern char *optarg;
	extern int optind;
	double delay = 5;

	char *viewstr = NULL;

	gid_t gid;
	int countmax = 0;
	int maxlines = 0;

	int ch;

	ut = open(_PATH_UTMP, O_RDONLY);
	if (ut < 0) {
		warn("No utmp");
	}

	kd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errbuf);

	gid = getgid();
	if (setresgid(gid, gid, gid) == -1)
		err(1, "setresgid");

	while ((ch = getopt(argc, argv, "abd:ins:w:")) != -1) {
		switch (ch) {
		case 'a':
			maxlines = -1;
			break;
		case 'b':
			rawmode = 1;
			interactive = 0;
			break;
		case 'd':
			countmax = atoi(optarg);
			if (countmax < 0)
				countmax = 0;
			break;
		case 'i':
			interactive = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 's':
			delay = atof(optarg);
			if (delay <= 0)
				delay = 5;
			break;
		case 'w':
			rawwidth = atoi(optarg);
			if (rawwidth < 1)
				rawwidth = DEFAULT_WIDTH;
			if (rawwidth >= MAX_LINE_BUF)
				rawwidth = MAX_LINE_BUF - 1;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	if (kd == NULL)
		warnx("kvm_openfiles: %s", errbuf);

	argc -= optind;
	argv += optind;

	if (argc == 1) {
		double del = atof(argv[0]);
		if (del == 0)
			viewstr = argv[0];
		else
			delay = del;
	} else if (argc == 2) {
		viewstr = argv[0];
		delay = atof(argv[1]);
		if (delay <= 0)
			delay = 5;
	}

	udelay = (useconds_t)(delay * 1000000.0);
	if (udelay < 1)
		udelay = 1;

	naptime = (double)udelay / 1000000.0;

	gethostname(hostname, sizeof (hostname));
	gethz();

	initialize();

	set_order(NULL);
	if (viewstr && set_view(viewstr)) {
		fprintf(stderr, "Unknown/ambiguous view name: %s\n", viewstr);
		return 1;
	}

	if (!isatty(STDOUT_FILENO)) {
		rawmode = 1;
		interactive = 0;
	}

	setup_term(maxlines);

	if (rawmode && countmax == 0)
		countmax = 1;

	gotsig_alarm = 1;

	engine_loop(countmax);

	return 0;
}
