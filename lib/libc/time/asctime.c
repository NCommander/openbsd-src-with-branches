/*
** This file is in the public domain, so clarified as of
** 1996-06-05 by Arthur David Olson (arthur_david_olson@nih.gov).
*/

#if defined(LIBC_SCCS) && !defined(lint) && !defined(NOID)
static char elsieid[] = "@(#)asctime.c	7.8";
static char rcsid[] = "$OpenBSD: asctime.c,v 1.4 1998/01/18 23:24:50 millert Exp $";
#endif /* LIBC_SCCS and not lint */

/*LINTLIBRARY*/

#include "private.h"
#include "tzfile.h"
#include "thread_private.h"

/*
** A la X3J11, with core dump avoidance.
*/

char *
asctime_r(timeptr, result)
register const struct tm *	timeptr;
char *result;
{
	static const char	wday_name[][3] = {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
	};
	static const char	mon_name[][3] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};

	register const char *	wn;
	register const char *	mn;
	int size;

	if (timeptr->tm_wday < 0 || timeptr->tm_wday >= DAYSPERWEEK)
		wn = "???";
	else	wn = wday_name[timeptr->tm_wday];
	if (timeptr->tm_mon < 0 || timeptr->tm_mon >= MONSPERYEAR)
		mn = "???";
	else	mn = mon_name[timeptr->tm_mon];
	/*
	** The X3J11-suggested format is
	**	"%.3s %.3s%3d %02.2d:%02.2d:%02.2d %d\n"
	** Since the .2 in 02.2d is ignored, we drop it.
	*/
	/*
	 * P1003 8.3.5.2 says that asctime_r() can only assume at most
	 * a 26 byte buffer.  *XXX*
	 */
	size = snprintf(result, 26, "%.3s %.3s%3d %02d:%02d:%02d %d\n",
		wn, mn,
		timeptr->tm_mday, timeptr->tm_hour,
		timeptr->tm_min, timeptr->tm_sec,
		TM_YEAR_BASE + timeptr->tm_year);
	if (size >= 26)
		return NULL;
	return result;
}

/*
 * Theoretically, the worst case string is of the form
 * "www mmm-2147483648 -2147483648:-2147483648:-2147483648 -2147483648\n"
 * but we only provide space for 26 since asctime_r won't use any more.
 * "www mmmddd hh:mm:ss yyyy\n"
 */
char *
asctime(timeptr)
const struct tm *	timeptr;
{

	static char result[26];
	_THREAD_PRIVATE_KEY(asctime)
	char *resultp = (char*) _THREAD_PRIVATE(asctime, result, NULL);

	if (resultp == NULL)
		return NULL;
	else
		return asctime_r(timeptr, resultp);
}

