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
**	lib_clear.c
**
**	The routine wclear().
**
*/

#include <curses.priv.h>

MODULE_ID("Id: lib_clear.c,v 1.4 1997/09/20 15:02:34 juergen Exp $")

int wclear(WINDOW *win)
{
int code = ERR;

	T((T_CALLED("wclear(%p)"), win));

	if ((code = werase(win))!=ERR)
	  win->_clear = TRUE;
	
	returnCode(code);
}
