/* Copyright (C) 1996 Free Software Foundation, Inc.
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
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include <sys/times.h>
#include <limits.h>
#if HAVE_UNISTD_H
#  include <unistd.h>
#endif
#include "f2c.h"

#ifdef KR_headers
int system_clock_(count, count_rate, count_max)
     integer *count, *count_rate, *count_max;
#else
int system_clock_(integer *count, integer *count_rate,
		   integer *count_max)
#endif
{
  struct tms buffer;
  unsigned long cnt;
#ifdef _SC_CLK_TCK
  *count_rate = sysconf(_SC_CLK_TCK);
#elif defined CLOCKS_PER_SECOND
  *count_rate = CLOCKS_PER_SECOND;
#elif defined CLK_TCK
  *count_rate = CLK_TCK;
#else
  #error Dont know clock tick length
#endif
  *count_max = INT_MAX;		/* dubious */
  cnt = times (&buffer);
  if (cnt > (unsigned long) (*count_max))
    *count = *count_max;	/* also dubious */
  else
    *count = cnt;
  return 0;
}
