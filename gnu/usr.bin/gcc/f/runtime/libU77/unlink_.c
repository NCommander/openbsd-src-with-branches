/* Copyright (C) 1995 Free Software Foundation, Inc.
This file is part of GNU Fortran libU77 library.

This library is free software; you can redistribute it and/or modify it
under the terms of the GNU Library General Public License as published
by the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GNU Fortran is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with GNU Fortran; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if HAVE_STDLIB_H
#  include <stdlib.h>
#else
#  include <stdio.h>
#endif
#if HAVE_UNISTD_H
#  include <unistd.h>
#endif
#include <errno.h>
#include <sys/param.h>
#include "f2c.h"

#ifdef KR_headers
integer unlink_ (str, Lstr)
     char *str; ftnlen  Lstr;
#else
integer unlink_ (const char *str, const ftnlen  Lstr)
#endif
{
  char *buff;
  char *bp, *blast;
  int i;

  buff = malloc (Lstr+1);
  if (buff == NULL) return -1;
  blast = buff + Lstr;
  for (bp = buff ; bp<blast ; )
    *bp++ = *str++;
  *bp = '\0';
  i = unlink (buff);
  free (buff);
  return i ? errno : 0;		/* SGI version returns -1 on failure. */
}
