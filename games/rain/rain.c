/*	$OpenBSD: rain.c,v 1.11 2002/05/31 03:40:01 pjanzen Exp $	*/

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
static char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)rain.c	8.1 (Berkeley) 5/31/93";
#else
static char rcsid[] = "$OpenBSD: rain.c,v 1.11 2002/05/31 03:40:01 pjanzen Exp $";
#endif
#endif /* not lint */

/*
 * rain 11/3/1980 EPS/CITHEP
 * cc rain.c -o rain -O -ltermlib
 */

#include <sys/types.h>
#include <curses.h>
#include <err.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

volatile sig_atomic_t sig_caught = 0;

static void	onsig(int);


int
main(argc, argv)
	int argc;
	char **argv;
{
	int x, y, j;
	long tcols, tlines;
	u_int delay = 0;
	int ch;
	int xpos[5], ypos[5];

	while ((ch = getopt(argc, argv, "d:h")) != -1)
		switch(ch) {
		case 'd':
			if ((delay = (u_int)strtoul(optarg,(char **)NULL,10)) < 1
				|| delay > 1000)
				errx(1, "invalid delay (1-1000)");
			delay *= 1000;  /* ms -> us */
			break;
		case 'h':
		default:
			(void)fprintf(stderr, "usage: rain [-d delay]\n");
			exit(1);
		}

	srandomdev();
	initscr();
	tcols = COLS - 4;
	tlines = LINES - 4;

	(void)signal(SIGHUP, onsig);
	(void)signal(SIGINT, onsig);
	(void)signal(SIGQUIT, onsig);
	(void)signal(SIGSTOP, onsig);
	(void)signal(SIGTSTP, onsig);
	(void)signal(SIGTERM, onsig);
	
	curs_set(0);
	for (j = 4; j >= 0; --j) {
		xpos[j] = random() % tcols + 2;
		ypos[j] = random() % tlines + 2;
	}
	for (j = 0;;) {
		if (sig_caught) {
			endwin();
			exit(0);
		}
		x = random() % tcols + 2;
		y = random() % tlines + 2;
		mvaddch(y, x, '.');
		mvaddch(ypos[j], xpos[j], 'o');
		if (!j--)
			j = 4;
		mvaddch(ypos[j], xpos[j], 'O');
		if (!j--)
			j = 4;
		mvaddch(ypos[j] - 1, xpos[j], '-');
		mvaddstr(ypos[j], xpos[j] - 1, "|.|");
		mvaddch(ypos[j] + 1, xpos[j], '-');
		if (!j--)
			j = 4;
		mvaddch(ypos[j] - 2, xpos[j], '-');
		mvaddstr(ypos[j] - 1, xpos[j] - 1, "/ \\");
		mvaddstr(ypos[j], xpos[j] - 2, "| O |");
		mvaddstr(ypos[j] + 1, xpos[j] - 1, "\\ /");
		mvaddch(ypos[j] + 2, xpos[j], '-');
		if (!j--)
			j = 4;
		mvaddch(ypos[j] - 2, xpos[j], ' ');
		mvaddstr(ypos[j] - 1, xpos[j] - 1, "   ");
		mvaddstr(ypos[j], xpos[j] - 2, "     ");
		mvaddstr(ypos[j] + 1, xpos[j] - 1, "   ");
		mvaddch(ypos[j] + 2, xpos[j], ' ');
		xpos[j] = x;
		ypos[j] = y;
		refresh();
		if (delay) usleep(delay);
	}
}

static void
onsig(dummy)
	int dummy;
{
	sig_caught = 1;
}
