/*	$OpenBSD: m_item_vis.c,v 1.4 1998/07/24 16:39:07 millert Exp $	*/

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
 *   Author: Juergen Pfeifer <juergen.pfeifer@gmx.net> 1995,1997            *
 ****************************************************************************/

/***************************************************************************
* Module m_item_vis                                                        *
* Tell if menu item is visible                                             *
***************************************************************************/

#include "menu.priv.h"

MODULE_ID("$From: m_item_vis.c,v 1.10 1999/05/16 17:26:34 juergen Exp $")

/*---------------------------------------------------------------------------
|   Facility      :  libnmenu  
|   Function      :  bool item_visible(const ITEM *item)
|   
|   Description   :  A item is visible if it currently appears in the
|                    subwindow of a posted menu.
|
|   Return Values :  TRUE  if visible
|                    FALSE if invisible
+--------------------------------------------------------------------------*/
bool item_visible(const ITEM * item)
{
  MENU *menu;
  
  if ( item                                               && 
      (menu=item->imenu)                                  && 
      (menu->status & _POSTED)                            &&
      ( (menu->toprow + menu->arows) > (item->y) )        &&
      ( item->y >= menu->toprow) )
    return TRUE;
  else
    return FALSE;
}

/* m_item_vis.c ends here */
