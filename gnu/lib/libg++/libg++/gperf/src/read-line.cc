/* Correctly reads an arbitrarily size string.

   Copyright (C) 1989 Free Software Foundation, Inc.
   written by Douglas C. Schmidt (schmidt@ics.uci.edu)

This file is part of GNU GPERF.

GNU GPERF is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GNU GPERF is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU GPERF; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111, USA.  */

#include <std.h>
#include "std-err.h"
#include "read-line.h"
#include "options.h"

/* Recursively fills up the buffer. */

char *
Read_Line::readln_aux (int chunks)
{
  T (Trace t ("Read_Line::readln_aux");)
#if LARGE_STACK_ARRAYS
  char buf[CHUNK_SIZE];
#else
  // Note: we don't use new, because that invokes a custom operator new.
  char *buf = (char*)malloc(CHUNK_SIZE);
  if (buf == NULL)
    abort ();
#endif
  char *bufptr = buf;
  char *ptr;
  int c;

  while ((c = getc (fp)) != EOF && c != '\n') /* fill the current buffer */
    {
      *bufptr++ = c;
      if (bufptr - buf >= CHUNK_SIZE) /* prepend remainder to ptr buffer */
        {
          if (ptr = readln_aux (chunks + 1))

            for (; bufptr != buf; *--ptr = *--bufptr);

          goto done;
        }
    }
  if (c == EOF && bufptr == buf && chunks == 0)
    ptr = NULL;
  else
    {
      c   = chunks * CHUNK_SIZE + bufptr - buf + 1;
      ptr = new char[c];

      for (*(ptr += (c - 1)) = '\0'; bufptr != buf; *--ptr = *--bufptr)
	;
    }
 done:
#if !LARGE_STACK_ARRAYS
  free (buf);
#endif

  return ptr;
}

#ifndef __OPTIMIZE__

/* Returns the ``next'' line, ignoring comments beginning with '#'. */

char *
Read_Line::get_line (void) 
{
  T (Trace t ("Read_Line::get_line");)
  int c;

  if ((c = getc (fp)) == '#')
    {
      while ((c = getc (fp)) != '\n' && c != EOF)
        ;

      return c != EOF ? get_line () : 0;
    }
  else
    {
      ungetc (c, stdin);
      return readln_aux (0);
    }
}
#endif
