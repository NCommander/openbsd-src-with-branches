/*	$OpenBSD: io.c,v 1.12 2003/06/03 02:56:17 millert Exp $	*/
/*	$NetBSD: io.c,v 1.4 1994/12/09 02:14:20 jtc Exp $	*/

/*
 * Copyright (c) 1983, 1993
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

#ifndef lint
#if 0
static char sccsid[] = "@(#)io.c	8.1 (Berkeley) 6/6/93";
#endif
static char rcsid[] = "$OpenBSD: io.c,v 1.12 2003/06/03 02:56:17 millert Exp $";
#endif /* not lint */

/*
 * This file contains the I/O handling and the exchange of
 * edit characters. This connection itself is established in
 * ctl.c
 */

#include "talk.h"
#include <sys/ioctl.h>
#include <sys/time.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#define A_LONG_TIME 10000000

volatile sig_atomic_t gotwinch = 0;

/*
 * The routine to do the actual talking
 */
void
talk(void)
{
	fd_set read_template, read_set;
	int nb;
	char buf[BUFSIZ];
	struct timeval wait;
	int maxfd = 0;

#if defined(NCURSES_VERSION) || defined(beep)
	message("Connection established");
	beep();
	beep();
	beep();
#else
	message("Connection established\007\007\007");
#endif
	current_line = 0;

	/*
	 * Wait on both the other process (sockt_mask) and
	 * standard input ( STDIN_MASK )
	 */
	FD_ZERO(&read_template);
	FD_SET(fileno(stdin), &read_template);
	if (fileno(stdin) > maxfd)
		maxfd = fileno(stdin);
	FD_SET(sockt, &read_template);
	if (sockt > maxfd)
		maxfd = sockt;
	for (;;) {
		read_set = read_template;
		wait.tv_sec = A_LONG_TIME;
		wait.tv_usec = 0;
		nb = select(maxfd + 1, &read_set, 0, 0, &wait);
		if (gotwinch) {
			resize_display();
			gotwinch = 0;
		}
		if (nb <= 0) {
			if (errno == EINTR) {
				read_set = read_template;
				continue;
			}
			/* panic, we don't know what happened */
			quit("Unexpected error from select", 1);
		}
		if (FD_ISSET(sockt, &read_set)) {
			/* There is data on sockt */
			nb = read(sockt, buf, sizeof buf);
			if (nb <= 0)
				quit("Connection closed.  Exiting", 0);
			display(&his_win, buf, nb);
		}
		if (FD_ISSET(fileno(stdin), &read_set)) {
			/*
			 * We can't make the tty non_blocking, because
			 * curses's output routines would screw up
			 */
			ioctl(0, FIONREAD, &nb);
			nb = read(0, buf, nb);
			display(&my_win, buf, nb);
			/* might lose data here because sockt is non-blocking */
			write(sockt, buf, nb);
		}
	}
}

/*
 * Display string in the standard location
 */
void
message(char *string)
{
	wmove(my_win.x_win, current_line % my_win.x_nlines, 0);
	wprintw(my_win.x_win, "[%s]", string);
	wclrtoeol(my_win.x_win);
	current_line++;
	wmove(my_win.x_win, current_line % my_win.x_nlines, 0);
	wrefresh(my_win.x_win);
}
