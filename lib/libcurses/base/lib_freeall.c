/*	$OpenBSD: lib_freeall.c,v 1.2 1998/11/17 03:16:21 millert Exp $	*/

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
 *  Author: Thomas E. Dickey <dickey@clark.net> 1996,1997                   *
 ****************************************************************************/

#include <curses.priv.h>
#include <term.h>

#if HAVE_NC_FREEALL

#if HAVE_LIBDBMALLOC
extern int malloc_errfd;	/* FIXME */
#endif

MODULE_ID("$From: lib_freeall.c,v 1.13 1998/11/12 19:42:42 Alexander.V.Lukyanov Exp $")

static void free_slk(SLK *p)
{
	if (p != 0) {
		FreeIfNeeded(p->ent);
		FreeIfNeeded(p->buffer);
		free(p);
	}
}

void _nc_free_termtype(struct termtype *p, int base)
{
	if (p != 0) {
		FreeIfNeeded(p->term_names);
		FreeIfNeeded(p->str_table);
		if (base)
			free(p);
	}
}

static void free_tries(struct tries *p)
{
	struct tries *q;

	while (p != 0) {
		q = p->sibling;
		if (p->child != 0)
			free_tries(p->child);
		free(p);
		p = q;
	}
}

/*
 * Free all ncurses data.  This is used for testing only (there's no practical
 * use for it as an extension).
 */
void _nc_freeall(void)
{
	WINDOWLIST *p, *q;

#if NO_LEAKS
	_nc_free_tparm();
#endif
	while (_nc_windows != 0) {
		/* Delete only windows that're not a parent */
		for (p = _nc_windows; p != 0; p = p->next) {
			bool found = FALSE;

			for (q = _nc_windows; q != 0; q = q->next) {
				if ((p != q)
				 && (q->win->_flags & _SUBWIN)
				 && (p->win == q->win->_parent)) {
					found = TRUE;
					break;
				}
			}

			if (!found) {
				delwin(p->win);
				break;
			}
		}
	}

	if (SP != 0) {
		free_tries (SP->_keytry);
		free_tries (SP->_key_ok);
	    	free_slk(SP->_slk);
		FreeIfNeeded(SP->_color_pairs);
		FreeIfNeeded(SP->_color_table);
		/* it won't free buffer anyway */
/*		_nc_set_buffer(SP->_ofp, FALSE);*/
#if !BROKEN_LINKER
		FreeAndNull(SP);
#endif
	}

	if (cur_term != 0) {
		_nc_free_termtype(&(cur_term->type), TRUE);
	}

#ifdef TRACE
	(void) _nc_trace_buf(-1, 0);
#endif
#if HAVE_LIBDBMALLOC
	malloc_dump(malloc_errfd);
#elif HAVE_LIBDMALLOC
#elif HAVE_PURIFY
	purify_all_inuse();
#endif
}

void _nc_free_and_exit(int code)
{
	_nc_freeall();
	exit(code);
}
#else
void _nc_freeall(void) { }
#endif
