/*	$OpenBSD$	*/

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
**	lib_options.c
**
**	The routines to handle option setting.
**
*/

#include <curses.priv.h>

#include <term.h>	/* keypad_xmit, keypad_local, meta_on, meta_off */
			/* cursor_visible,cursor_normal,cursor_invisible */

MODULE_ID("$From: lib_options.c,v 1.31 1998/04/11 23:40:51 tom Exp $")

bool has_ic(void)
{
	T((T_CALLED("has_ic()")));
	returnCode((insert_character || parm_ich
	   ||  (enter_insert_mode && exit_insert_mode))
	   &&  (delete_character || parm_dch));
}

bool has_il(void)
{
	T((T_CALLED("has_il()")));
	returnCode((insert_line || parm_insert_line)
		&& (delete_line || parm_delete_line));
}

int idlok(WINDOW *win,  bool flag)
{
	T((T_CALLED("idlok(%p,%d)"), win, flag));

	if (win) {
	  _nc_idlok = win->_idlok = flag && (has_il() || change_scroll_region);
	  returnCode(OK);
	}
	else
	  returnCode(ERR);
}


void idcok(WINDOW *win, bool flag)
{
	T((T_CALLED("idcok(%p,%d)"), win, flag));

	if (win)
	  _nc_idcok = win->_idcok = flag && has_ic();

	returnVoid;
}

int halfdelay(int t)
{
	T((T_CALLED("halfdelay(%d)"), t));

	if (t < 1 || t > 255)
		returnCode(ERR);

	cbreak();
	SP->_cbreak = t+1;
	returnCode(OK);
}

int nodelay(WINDOW *win, bool flag)
{
	T((T_CALLED("nodelay(%p,%d)"), win, flag));

	if (win) {
	  if (flag == TRUE)
	    win->_delay = 0;
	  else win->_delay = -1;
	  returnCode(OK);
	}
	else
	  returnCode(ERR);
}

int notimeout(WINDOW *win, bool f)
{
	T((T_CALLED("notimout(%p,%d)"), win, f));

	if (win) {
	  win->_notimeout = f;
	  returnCode(OK);
	}
	else
	  returnCode(ERR);
}

void wtimeout(WINDOW *win, int delay)
{
	T((T_CALLED("wtimeout(%p,%d)"), win, delay));

	if (win) {
	  win->_delay = delay;
	}
}

int keypad(WINDOW *win, bool flag)
{
	T((T_CALLED("keypad(%p,%d)"), win, flag));

	if (win) {
	  win->_use_keypad = flag;
	  returnCode(_nc_keypad(flag));
	}
	else
	  returnCode(ERR);
}


int meta(WINDOW *win GCC_UNUSED, bool flag)
{
        /* Ok, we stay relaxed and don't signal an error if win is NULL */
	T((T_CALLED("meta(%p,%d)"), win, flag));

	SP->_use_meta = flag;

	if (flag  &&  meta_on)
	{
	    TPUTS_TRACE("meta_on");
	    putp(meta_on);
	}
	else if (! flag  &&  meta_off)
	{
	    TPUTS_TRACE("meta_off");
	    putp(meta_off);
	}
	returnCode(OK);
}

/* curs_set() moved here to narrow the kernel interface */

int curs_set(int vis)
{
int cursor = SP->_cursor;

	T((T_CALLED("curs_set(%d)"), vis));

	if (vis < 0 || vis > 2)
		returnCode(ERR);

	if (vis == cursor)
		returnCode(cursor);

	switch(vis) {
	case 2:
		if (cursor_visible)
		{
			TPUTS_TRACE("cursor_visible");
			putp(cursor_visible);
		}
		else
			returnCode(ERR);
		break;
	case 1:
		if (cursor_normal)
		{
			TPUTS_TRACE("cursor_normal");
			putp(cursor_normal);
		}
		else
			returnCode(ERR);
		break;
	case 0:
		if (cursor_invisible)
		{
			TPUTS_TRACE("cursor_invisible");
			putp(cursor_invisible);
		}
		else
			returnCode(ERR);
		break;
	}
	SP->_cursor = vis;
	(void) fflush(SP->_ofp);

	returnCode(cursor==-1 ? 1 : cursor);
}

int typeahead(int fd)
{
	T((T_CALLED("typeahead(%d)"), fd));
	SP->_checkfd = fd;
	returnCode(OK);
}

/*
**      has_key()
**
**      Return TRUE if the current terminal has the given key
**
*/

#ifdef NCURSES_EXT_FUNCS
static int has_key_internal(int keycode, struct tries *tp)
{
    if (tp == 0)
	return(FALSE);
    else if (tp->value == keycode)
	return(TRUE);
    else
	return(has_key_internal(keycode, tp->child)
	       || has_key_internal(keycode, tp->sibling));
}

int has_key(int keycode)
{
    T((T_CALLED("has_key(%d)"), keycode));
    returnCode(has_key_internal(keycode, SP->_keytry));
}
#endif /* NCURSES_EXT_FUNCS */

/*
**      init_keytry()
**
**      Construct the try for the current terminal's keypad keys.
**
*/

static void init_keytry(void)
{
/* LINT_PREPRO
#if 0*/
#include <keys.tries>
/* LINT_PREPRO
#endif*/
	size_t n;

	/* The SP->_keytry value is initialized in newterm(), where the SP
	 * structure is created, because we can not tell where keypad() or
	 * mouse_activate() (which will call keyok()) are first called.
	 */

	for (n = 0; n < SIZEOF(table); n++)
		if (table[n].offset < STRCOUNT)
		_nc_add_to_try(&(SP->_keytry),
			CUR Strings[table[n].offset],
			table[n].code);
}

/* Turn the keypad on/off
 *
 * Note:  we flush the output because changing this mode causes some terminals
 * to emit different escape sequences for cursor and keypad keys.  If we don't
 * flush, then the next wgetch may get the escape sequence that corresponds to
 * the terminal state _before_ switching modes.
 */
int _nc_keypad(bool flag)
{
	if (flag  &&  keypad_xmit)
	{
	    TPUTS_TRACE("keypad_xmit");
	    putp(keypad_xmit);
	    (void) fflush(SP->_ofp);
	}
	else if (! flag  &&  keypad_local)
	{
	    TPUTS_TRACE("keypad_local");
	    putp(keypad_local);
	    (void) fflush(SP->_ofp);
	}

	if (!SP->_tried) {
	    init_keytry();
	    SP->_tried = TRUE;
	}
	return(OK);
}
