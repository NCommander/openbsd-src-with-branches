/*	$OpenBSD: panel.priv.h,v 1.3 1998/07/24 17:08:24 millert Exp $	*/
/* $From: panel.priv.h,v 1.12 1999/11/25 13:49:26 juergen Exp $ */

#ifndef _PANEL_PRIV_H
#define _PANEL_PRIV_H

#if HAVE_CONFIG_H
#  include <ncurses_cfg.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if HAVE_LIBDMALLOC
#  include <dmalloc.h>    /* Gray Watson's library */
#endif

#if HAVE_LIBDBMALLOC
#  include <dbmalloc.h>   /* Conor Cahill's library */
#endif

#include <nc_panel.h>
#include "panel.h"

#if ( CC_HAS_INLINE_FUNCS && !defined(TRACE) )
#  define INLINE inline
#else
#  define INLINE
#endif

#ifdef USE_RCS_IDS
#  define MODULE_ID(id) static const char Ident[] = id;
#else
#  define MODULE_ID(id) /*nothing*/
#endif


#ifdef TRACE
   extern const char *_nc_my_visbuf(const void *);
#  ifdef TRACE_TXT
#    define USER_PTR(ptr) _nc_visbuf((const char *)ptr)
#  else
#    define USER_PTR(ptr) _nc_my_visbuf((const char *)ptr)
#  endif

   extern void _nc_dPanel(const char*, const PANEL*);
   extern void _nc_dStack(const char*, int, const PANEL*);
   extern void _nc_Wnoutrefresh(const PANEL*);
   extern void _nc_Touchpan(const PANEL*);
   extern void _nc_Touchline(const PANEL*, int, int);

#  define dBug(x) _tracef x
#  define dPanel(text,pan) _nc_dPanel(text,pan)
#  define dStack(fmt,num,pan) _nc_dStack(fmt,num,pan)
#  define Wnoutrefresh(pan) _nc_Wnoutrefresh(pan)
#  define Touchpan(pan) _nc_Touchpan(pan)
#  define Touchline(pan,start,count) _nc_Touchline(pan,start,count)
#else /* !TRACE */
#  define dBug(x)
#  define dPanel(text,pan)
#  define dStack(fmt,num,pan)
#  define Wnoutrefresh(pan) wnoutrefresh((pan)->win)
#  define Touchpan(pan) touchwin((pan)->win)
#  define Touchline(pan,start,count) touchline((pan)->win,start,count)
#endif

#define _nc_stdscr_pseudo_panel _nc_panelhook()->stdscr_pseudo_panel
#define _nc_top_panel _nc_panelhook()->top_panel
#define _nc_bottom_panel _nc_panelhook()->bottom_panel

#define EMPTY_STACK() (_nc_top_panel==_nc_bottom_panel)
#define Is_Bottom(p)  (((p)!=(PANEL*)0) && !EMPTY_STACK() && (_nc_bottom_panel->above==(p))) 
#define Is_Top(p) (((p)!=(PANEL*)0) && !EMPTY_STACK() && (_nc_top_panel==(p)))
#define Is_Pseudo(p) ((p) && ((p)==_nc_bottom_panel))

/* borrowed from curses.priv.h */
#define CHANGED_RANGE(line,start,end) \
	if (line->firstchar == _NOCHANGE \
	 || line->firstchar > (start)) \
		line->firstchar = start; \
	if (line->lastchar == _NOCHANGE \
	 || line->lastchar < (end)) \
		line->lastchar = end

/*+-------------------------------------------------------------------------
	IS_LINKED(pan) - check to see if panel is in the stack
--------------------------------------------------------------------------*/
/* This works! The only case where it would fail is, when the list has
   only one element. But this could only be the pseudo panel at the bottom */
#define IS_LINKED(p) (((p)->above || (p)->below ||((p)==_nc_bottom_panel)) ? TRUE : FALSE)

#define PSTARTX(pan) ((pan)->win->_begx)
#define PENDX(pan)   ((pan)->win->_begx + getmaxx((pan)->win) - 1)
#define PSTARTY(pan) ((pan)->win->_begy)
#define PENDY(pan)   ((pan)->win->_begy + getmaxy((pan)->win) - 1)

/*+-------------------------------------------------------------------------
	PANELS_OVERLAPPED(pan1,pan2) - check panel overlapped
---------------------------------------------------------------------------*/
#define PANELS_OVERLAPPED(pan1,pan2) \
(( !(pan1) || !(pan2) || \
       PSTARTY(pan1) > PENDY(pan2) || PENDY(pan1) < PSTARTY(pan2) ||\
       PSTARTX(pan1) > PENDX(pan2) || PENDX(pan1) < PSTARTX(pan2) ) \
     ? FALSE : TRUE)


/*+-------------------------------------------------------------------------
	Compute the intersection rectangle of two overlapping rectangles
---------------------------------------------------------------------------*/
#define COMPUTE_INTERSECTION(pan1,pan2,ix1,ix2,iy1,iy2)\
   ix1 = (PSTARTX(pan1) < PSTARTX(pan2)) ? PSTARTX(pan2) : PSTARTX(pan1);\
   ix2 = (PENDX(pan1)   < PENDX(pan2))   ? PENDX(pan1)   : PENDX(pan2);\
   iy1 = (PSTARTY(pan1) < PSTARTY(pan2)) ? PSTARTY(pan2) : PSTARTY(pan1);\
   iy2 = (PENDY(pan1)   < PENDY(pan2))   ? PENDY(pan1)   : PENDY(pan2);\
   assert((ix1<=ix2) && (iy1<=iy2));\


/*+-------------------------------------------------------------------------
	Walk through the panel stack starting at the given location and
        check for intersections; overlapping panels are "touched", so they
        are incrementally overwriting cells that should be hidden. 
        If the "touch" flag is set, the panel gets touched before it is
        updated. 
---------------------------------------------------------------------------*/
#define PANEL_UPDATE(pan,panstart,touch)\
{  int y;\
   PANEL* pan2 = ((panstart) ? (panstart) : _nc_bottom_panel);\
   if (touch)\
      Touchpan(pan);\
   while(pan2) {\
      if ((pan2 != pan) && PANELS_OVERLAPPED(pan,pan2)) {\
        int ix1,ix2,iy1,iy2;\
        COMPUTE_INTERSECTION(pan,pan2,ix1,ix2,iy1,iy2);\
	for(y = iy1; y <= iy2; y++) {\
	  if (is_linetouched(pan->win,y - PSTARTY(pan))) {\
            struct ldat* line = &(pan2->win->_line[y - PSTARTY(pan2)]);\
            CHANGED_RANGE(line,ix1-PSTARTX(pan2),ix2-PSTARTX(pan2));\
          }\
	}\
      }\
      pan2 = pan2->above;\
   }\
}

/*+-------------------------------------------------------------------------
	Remove panel from stack.
---------------------------------------------------------------------------*/
#define PANEL_UNLINK(pan,err) \
{  err = ERR;\
   if (pan) {\
     if (IS_LINKED(pan)) {\
       if ((pan)->below)\
         (pan)->below->above = (pan)->above;\
       if ((pan)->above)\
         (pan)->above->below = (pan)->below;\
       if ((pan) == _nc_bottom_panel) \
         _nc_bottom_panel = (pan)->above;\
       if ((pan) == _nc_top_panel) \
         _nc_top_panel = (pan)->below;\
       err = OK;\
     }\
     (pan)->above = (pan)->below = (PANEL*)0;\
   }\
}

#define HIDE_PANEL(pan,err,err_if_unlinked)\
  if (IS_LINKED(pan)) {\
    PANEL_UPDATE(pan,(PANEL*)0,TRUE);\
    PANEL_UNLINK(pan,err);\
  } \
  else {\
    if (err_if_unlinked)\
      err = ERR;\
  }

#endif /* _PANEL_PRIV_H */
