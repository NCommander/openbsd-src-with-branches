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


/* This definitely shouldn't be done this way -- should canibalise
   chmod(1) from GNU or BSD. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#if HAVE_UNISTD_H
#  include <unistd.h>
#endif
#if HAVE_STDLIB_H
#  include <stdlib.h>
#else
#  include <stdio.h>		/* for NULL */
#endif

#include "f2c.h"

#ifndef CHMOD_PATH
#define CHMOD_PATH "/bin/chmod"
#endif

#ifdef KR_headers
extern void s_cat ();

integer chmod_ (name, mode, Lname, Lmode)
     char *name, *mode;
     ftnlen Lname, Lmode;
#else
extern void s_cat(char *lp, char *rpp[], ftnlen rnp[], ftnlen *np, ftnlen ll);

integer chmod_ (/* const */ char *name, /* const */ char *mode,
	        const ftnlen  Lname, const ftnlen Lmode)
#endif
{
  char *buff;
  char *bp, *blast;
  int i, l;
  ftnlen six = 6;
  address a[6];
  integer ii[6];
  char chmod_path [] = CHMOD_PATH;
  l = strlen (chmod_path);
  buff = malloc (Lname+Lmode+l+2+12+1);
  if (buff == NULL) return -1;
  ii[0] = l; a[0] = chmod_path;
  ii[1] = 1; a[1] = " ";
  ii[2] = Lmode; a[2] = mode;
  ii[3] = 1; a[3] = " ";
  ii[4] = Lname; a[4] = name;
  ii[5] = 12; a[5] = " 2>/dev/null";
  s_cat (buff, a, ii, &six, Lname+Lmode+l+2+12);
  buff[Lname+Lmode+l+2+12] = '\0';
  i = system (buff);
  free (buff);
  return i;
}
