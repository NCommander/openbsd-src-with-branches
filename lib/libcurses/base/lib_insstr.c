/*	$OpenBSD: lib_insstr.c,v 1.4 1998/07/23 21:18:55 millert Exp $	*/

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
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/



/*
**	lib_insstr.c
**
**	The routine winsnstr().
**
*/

#include <curses.priv.h>
#include <ctype.h>

MODULE_ID("$From: lib_insstr.c,v 1.12 1998/02/23 12:12:16 tom Exp $")

int winsnstr(WINDOW *win, const char *s, int n)
{
int     code = ERR;
short	oy;
short	ox ;
const unsigned char *str = (const unsigned char *)s;
const unsigned char *cp;

        T((T_CALLED("winsnstr(%p,%s,%d)"), win, _nc_visbuf(str), n));

	if (win && str) {
	  oy = win->_cury; ox = win->_curx;
	  for (cp = str; *cp && (n <= 0 || (cp - str) < n); cp++) {
	    if (*cp == '\n' || *cp == '\r' || *cp == '\t' || *cp == '\b')
	      _nc_waddch_nosync(win, (chtype)(*cp));
	    else if (is7bits(*cp) && iscntrl(*cp)) {
	      winsch(win, ' ' + (chtype)(*cp));
	      winsch(win, '^');
	      win->_curx += 2;
	    } else {
	      winsch(win, (chtype)(*cp));
	      win->_curx++;
	    }
	    if (win->_curx > win->_maxx)
	      win->_curx = win->_maxx;
	  }
	  
	  win->_curx = ox;
	  win->_cury = oy;
	  _nc_synchook(win);
	  code = OK;
	}
	returnCode(code);
}
