/*	$OpenBSD: termcap.h,v 1.4 1999/05/08 20:28:59 millert Exp $	*/

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

/* $From: termcap.h.in,v 1.10 1999/01/09 22:38:04 Uchiyama.Yasushi Exp $ */

#ifndef _NCU_TERMCAP_H
#define _NCU_TERMCAP_H	1

#undef  NCURSES_VERSION
#define NCURSES_VERSION "5.1"

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <sys/types.h>
#include <termios.h> 

#undef  NCURSES_CONST 
#define NCURSES_CONST  

extern char PC;
extern char *UP;
extern char *BC;
extern speed_t ospeed; 

#if !defined(_NCU_TERM_H)
extern char *tgetstr(NCURSES_CONST char *, char **);
extern char *tgoto(const char *, int, int);
extern int tgetent(char *, const char *);
extern int tgetflag(NCURSES_CONST char *);
extern int tgetnum(NCURSES_CONST char *);
extern int tputs(const char *, int, int (*)(int));
#endif

#ifdef __cplusplus
}
#endif

#endif /* _NCU_TERMCAP_H */
