/*	$OpenBSD: home_terminfo.c,v 1.1 1998/10/31 06:30:29 millert Exp $	*/

/****************************************************************************
 * Copyright (c) 1998 Free Software Foundation, Inc.                        *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Thomas E. Dickey <dickey@clark.net> 1998                        *
 ****************************************************************************/

/*
 *	home_terminfo.c -- return the $HOME/.terminfo string, expanded
 */

#include <curses.priv.h>
#include <tic.h>

MODULE_ID("$From: home_terminfo.c,v 1.1 1998/09/19 21:25:03 tom Exp $")

#define my_length (strlen(home) + sizeof(PRIVATE_INFO))

/* ncurses extension...fall back on user's private directory */

char *
_nc_home_terminfo(void)
{
	char *home;
	static char *temp = 0;

	if (temp == 0) {
		if ((home = getenv("HOME")) != 0
		 && my_length <= PATH_MAX) {
			temp = malloc(my_length);
			if (temp == 0)
				_nc_err_abort("Out of memory");
			(void) sprintf(temp, PRIVATE_INFO, home);
		}
	}
	return temp;
}
