/*	$OpenBSD$	*/


/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992-1995                        *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of ncurses in any   *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/

/*
**	lib_clrbot.c
**
**	The routine wclrtobot().
**
*/

#include <curses.priv.h>

MODULE_ID("Id: lib_clrbot.c,v 1.12 1997/09/20 15:02:34 juergen Exp $")

int wclrtobot(WINDOW *win)
{
int     code = ERR;
chtype	blank;
chtype	*ptr, *end;
short	y, startx;

	T((T_CALLED("wclrtobot(%p)"), win));

	if (win) {
	  startx = win->_curx;
	  
	  T(("clearing from y = %d to y = %d with maxx =  %d", win->_cury, win->_maxy, win->_maxx));
	  
	  for (y = win->_cury; y <= win->_maxy; y++) {
	    end = &win->_line[y].text[win->_maxx];
	    
	    blank = _nc_background(win);
	    for (ptr = &win->_line[y].text[startx]; ptr <= end; ptr++)
	      *ptr = blank;
	    
	    if (win->_line[y].firstchar > startx
		||  win->_line[y].firstchar == _NOCHANGE)
	      win->_line[y].firstchar = startx;
	    
	    win->_line[y].lastchar = win->_maxx;
	    
	    startx = 0;
	  }
	  _nc_synchook(win);
	  code = OK;
	}
	returnCode(code);
}

