/*	$Id$	*/

/*-
 * Copyright 1987, 1988 by the Student Information Processing Board
 *	of the Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose and without fee is
 * hereby granted, provided that the above copyright notice
 * appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation,
 * and that the names of M.I.T. and the M.I.T. S.I.P.B. not be
 * used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 * M.I.T. and the M.I.T. S.I.P.B. make no representations about
 * the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 */

/*
 * Convert a struct tm * to a UNIX time.
 */

#include <adm_locl.h>

#define daysinyear(y) (((y) % 4) ? 365 : (((y) % 100) ? 366 : (((y) % 400) ? 365 : 366)))

#define SECSPERDAY 24*60*60
#define SECSPERHOUR 60*60
#define SECSPERMIN 60

static int cumdays[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334,
			     365};

static int leapyear[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static int nonleapyear[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

long
maketime(struct tm *tp, int local)
{
    register long retval;
    int foo;
    int *marray;

    if (tp->tm_mon < 0 || tp->tm_mon > 11 ||
	tp->tm_hour < 0 || tp->tm_hour > 23 ||
	tp->tm_min < 0 || tp->tm_min > 59 ||
	tp->tm_sec < 0 || tp->tm_sec > 59) /* out of range */
	return 0;

    retval = 0;
    if (tp->tm_year < 1900)
	foo = tp->tm_year + 1900;
    else
	foo = tp->tm_year;

    if (foo < 1901 || foo > 2038)	/* year is too small/large */
	return 0;

    if (daysinyear(foo) == 366) {
	if (tp->tm_mon > 1)
	    retval+= SECSPERDAY;	/* add leap day */
	marray = leapyear;
    } else
	marray = nonleapyear;

    if (tp->tm_mday < 0 || tp->tm_mday > marray[tp->tm_mon])
	return 0;			/* out of range */

    while (--foo >= 1970)
	retval += daysinyear(foo) * SECSPERDAY;

    retval += cumdays[tp->tm_mon] * SECSPERDAY;
    retval += (tp->tm_mday-1) * SECSPERDAY;
    retval += tp->tm_hour * SECSPERHOUR + tp->tm_min * SECSPERMIN + tp->tm_sec;

    if (local) {
	/* need to use local time, so we retrieve timezone info */
	struct timezone tz;
	struct timeval tv;
	if (gettimeofday(&tv, &tz) < 0) {
	    /* some error--give up? */
	    return(retval);
	}
	retval += tz.tz_minuteswest * SECSPERMIN;
    }
    return(retval);
}
