/* Macro definitions for Sparc running under OpenBSD.
   Copyright 1994 Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef TM_OBSD_H
#define TM_OBSD_H

#ifdef __arch64__
#include "sparc/tm-sp64.h"
#else
#include "sparc/tm-sparc.h"
#endif

#include "tm-obsd.h"

#endif /* TM_OBSD_H */
