/*	$OpenBSD: day.c,v 1.5 1998/11/04 11:32:02 pjanzen Exp $	*/

/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static const char sccsid[] = "@(#)calendar.c  8.3 (Berkeley) 3/25/94";
#else
static char rcsid[] = "$OpenBSD: day.c,v 1.5 1998/11/04 11:32:02 pjanzen Exp $";
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/uio.h>

#include <ctype.h>
#include <err.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tzfile.h>

#include "pathnames.h"
#include "calendar.h"

struct tm *tp;
int *cumdays, offset, yrdays;
char dayname[10];


/* 1-based month, 0-based days, cumulative */
int daytab[][14] = {
	{ 0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, 364 },
	{ 0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
};

static char *days[] = {
	"sun", "mon", "tue", "wed", "thu", "fri", "sat", NULL,
};

static char *months[] = {
	"jan", "feb", "mar", "apr", "may", "jun",
	"jul", "aug", "sep", "oct", "nov", "dec", NULL,
};

static struct fixs fndays[8];         /* full national days names */
static struct fixs ndays[8];          /* short national days names */

static struct fixs fnmonths[13];      /* full national months names */
static struct fixs nmonths[13];       /* short national month names */


void setnnames(void)
{
	char buf[80];
	int i, l;
	struct tm tm;

	for (i = 0; i < 7; i++) {
		tm.tm_wday = i;
		l = strftime(buf, sizeof(buf), "%a", &tm);
		for (; l > 0 && isspace((int)buf[l - 1]); l--)
			;
		buf[l] = '\0';
		if (ndays[i].name != NULL)
			free(ndays[i].name);
		if ((ndays[i].name = strdup(buf)) == NULL)
			errx(1, "cannot allocate memory");
		ndays[i].len = strlen(buf);

		l = strftime(buf, sizeof(buf), "%A", &tm);
		for (; l > 0 && isspace((int)buf[l - 1]); l--)
			;
		buf[l] = '\0';
		if (fndays[i].name != NULL)
			free(fndays[i].name);
		if ((fndays[i].name = strdup(buf)) == NULL)
			errx(1, "cannot allocate memory");
		fndays[i].len = strlen(buf);
	}

	for (i = 0; i < 12; i++) {
		tm.tm_mon = i;
		l = strftime(buf, sizeof(buf), "%b", &tm);
		for (; l > 0 && isspace((int)buf[l - 1]); l--)
			;
		buf[l] = '\0';
		if (nmonths[i].name != NULL)
			free(nmonths[i].name);
		if ((nmonths[i].name = strdup(buf)) == NULL)
			errx(1, "cannot allocate memory");
		nmonths[i].len = strlen(buf);

		l = strftime(buf, sizeof(buf), "%B", &tm);
		for (; l > 0 && isspace((int)buf[l - 1]); l--)
			;
		buf[l] = '\0';
		if (fnmonths[i].name != NULL)
			free(fnmonths[i].name);
		if ((fnmonths[i].name = strdup(buf)) == NULL)
			errx(1, "cannot allocate memory");
		fnmonths[i].len = strlen(buf);
	}
}

void
settime(now)
    	time_t now;
{
	tp = localtime(&now);
	if (isleap(tp->tm_year + TM_YEAR_BASE)) {
		yrdays = DAYSPERLYEAR;
		cumdays = daytab[1];
	} else {
		yrdays = DAYSPERNYEAR;
		cumdays = daytab[0];
	}
	/* Friday displays Monday's events */
	offset = tp->tm_wday == 5 ? 3 : 1;
	header[5].iov_base = dayname;

	(void) setlocale(LC_TIME, "C");
	header[5].iov_len = strftime(dayname, sizeof(dayname), "%A", tp);
	(void) setlocale(LC_TIME, "");

	setnnames();
}

/* convert [Year][Month]Day into unix time (since 1970)
 * Year: two or four digits, Month: two digits, Day: two digits
 */
time_t Mktime (date)
    char *date;
{
    time_t t;
    int len;
    struct tm tm;

    (void)time(&t);
    tp = localtime(&t);

    len = strlen(date);
    if (len < 2)
	return((time_t)-1);
    tm.tm_sec = 0;
    tm.tm_min = 0;
    /* Avoid getting caught by a timezone shift; set time to noon */
    tm.tm_isdst = 0;
    tm.tm_hour = 12;
    tm.tm_wday = 0;
    tm.tm_mday = tp->tm_mday;
    tm.tm_mon = tp->tm_mon;
    tm.tm_year = tp->tm_year;

    /* Day */
    tm.tm_mday = atoi(date + len - 2);

    /* Month */
    if (len >= 4) {
	*(date + len - 2) = '\0';
	tm.tm_mon = atoi(date + len - 4) - 1;
    }

    /* Year */
    if (len >= 6) {
	*(date + len - 4) = '\0';
	tm.tm_year = atoi(date);

	/* tm_year up TM_YEAR_BASE ... */
	if (tm.tm_year < 69)		/* Y2K */
		tm.tm_year += 2000 - TM_YEAR_BASE;
	else if (tm.tm_year < 100)
		tm.tm_year += 1900 - TM_YEAR_BASE;
	else if (tm.tm_year > TM_YEAR_BASE)
		tm.tm_year -= TM_YEAR_BASE;
    }

#if DEBUG
    printf("Mktime: %d %d %d %s\n", (int)mktime(&tm), (int)t, len,
	   asctime(&tm));
#endif
    return(mktime(&tm));
}

/*
 * Possible date formats include any combination of:
 *	3-charmonth			(January, Jan, Jan)
 *	3-charweekday			(Friday, Monday, mon.)
 *	numeric month or day		(1, 2, 04)
 *
 * Any character may separate them, or they may not be separated.  Any line,
 * following a line that is matched, that starts with "whitespace", is shown
 * along with the matched line.
 */
struct match *
isnow(endp)
	char	*endp;
{
	int day, flags = 0, month = 0, v1, v2;
	int monthp, dayp, varp;
	struct match *matches;

	/*
	 * CONVENTION
	 *
	 * Month:     1-12
	 * Monthname: Jan .. Dec
	 * Day:       1-31
	 * Weekday:   Mon-Sun
	 *
	 */

	/* read first field */
	/* didn't recognize anything, skip it */
	if (!(v1 = getfield(endp, &endp, &flags)))
		return (NULL);

	/* Easter or Easter depending days */
	if (flags & F_EASTER)
	    day = v1 - 1; /* days since January 1 [0-365] */

	 /*
	  * 1. {Weekday,Day} XYZ ...
	  *
	  *    where Day is > 12
	  */
	else if (flags & F_ISDAY || v1 > 12) {

		/* found a day; day: 1-31 or weekday: 1-7 */
		day = v1;

		/* {Day,Weekday} {Month,Monthname} ... */
		/* if no recognizable month, assume just a day alone
		 * in other words, find month or use current month */
		if (!(month = getfield(endp, &endp, &flags)))
			month = tp->tm_mon + 1;
	}

	/* 2. {Monthname} XYZ ... */
	else if (flags & F_ISMONTH) {
		month = v1;

		/* Monthname {day,weekday} */
		/* if no recognizable day, assume the first day in month */
		if (!(day = getfield(endp, &endp, &flags)))
			day = 1;
	}

	/* Hm ... */
	else {
		v2 = getfield(endp, &endp, &flags);

		/*
		 * {Day} {Monthname} ...
		 * where Day <= 12
		 */
		if (flags & F_ISMONTH) {
			day = v1;
			month = v2;
			varp = 0;
		}

		/* {Month} {Weekday,Day} ...  */
		else {
			/* F_ISDAY set, v2 > 12, or no way to tell */
			month = v1;
			/* if no recognizable day, assume the first */
			day = v2 ? v2 : 1;
			varp = 0;
		}
	}

	/* convert Weekday into *next*  Day,
	 * e.g.: 'Sunday' -> 22
	 *       'SundayLast' -> ??
	 */
	if (flags & F_ISDAY) {
#if DEBUG
	    fprintf(stderr, "\nday: %d %s month %d\n", day, endp, month);
#endif

	    varp = 1;
	    /* variable weekday, SundayLast, MondayFirst ... */
	    if (day < 0 || day >= 10) {

		/* negative offset; last, -4 .. -1 */
		if (day < 0) {
		    v1 = day/10 - 1;          /* offset -4 ... -1 */
		    day = 10 + (day % 10);    /* day 1 ... 7 */

		    /* which weekday the end of the month is (1-7) */
		    v2 = (cumdays[month + 1] - tp->tm_yday +
		        tp->tm_wday + 371) % 7 + 1;

		    /* and subtract enough days */
		    day = cumdays[month + 1] - cumdays[month] +
		        (v1 + 1) * 7 - (v2 - day + 7) % 7;
#if DEBUG
		    fprintf(stderr, "\nMonth %d ends on weekday %d\n", month, v2);
#endif
		}

		/* first, second ... +1 ... +5 */
		else {
		    v1 = day/10;        /* offset */
		    day = day % 10;

		    /* which weekday the first of the month is */
		    v2 = (cumdays[month] - tp->tm_yday +
		        tp->tm_wday + 372) % 7 + 1;
		    
		    /* and add enough days */
		    day = 1 + (v1 - 1) * 7 + (day - v2 + 7) % 7;
#if DEBUG
		    fprintf(stderr, "\nMonth %d starts on weekday %d\n", month, v2);
#endif
	    }
	    }
	    else
		    day = tp->tm_mday + (((day - 1) - tp->tm_wday + 7) % 7);
	}

	if (!(flags & F_EASTER)) {
	    monthp = month;
	    dayp = day;
	    day = cumdays[month] + day;
	}
	else {
	    for (v1 = 0; day > cumdays[v1]; v1++)
		;
	    monthp = v1 - 1;
	    dayp = day - cumdays[v1 - 1];
	    varp = 1;
	}

#if DEBUG
	fprintf(stderr, "day2: day %d(%d) yday %d\n", dayp, day, tp->tm_yday);
#endif
	/* if today or today + offset days */
	if ((day >= tp->tm_yday - f_dayBefore &&
	    day <= tp->tm_yday + offset + f_dayAfter) ||

	/* if number of days left in this year + days to event in next year */
	   (yrdays - tp->tm_yday + day <= offset + f_dayAfter ||
	    /* a year backward, eg. 6 Jan and 10 days before -> 27. Dec */
	    tp->tm_yday + day - f_dayBefore < 0
	    )) {
		if ((matches = malloc(sizeof(struct match))) == NULL)
			errx(1,"cannot allocate memory");
		matches->month = monthp;
		matches->day   = dayp;
		matches->var   = varp;
		matches->year  = tp->tm_year;	/* XXX */
		matches->next  = NULL;
		return (matches);
	}
	return (NULL);
}


int
getmonth(s)
	register char *s;
{
	register char **p;
	struct fixs *n;

	for (n = fnmonths; n->name; ++n)
		if (!strncasecmp(s, n->name, n->len))
			return ((n - fnmonths) + 1);
	for (n = nmonths; n->name; ++n)
		if (!strncasecmp(s, n->name, n->len))
			return ((n - nmonths) + 1);
	for (p = months; *p; ++p)
		if (!strncasecmp(s, *p, 3))
			return ((p - months) + 1);
	return (0);
}


int
getday(s)
	register char *s;
{
	register char **p;
	struct fixs *n;

	for (n = fndays; n->name; ++n)
		if (!strncasecmp(s, n->name, n->len))
			return ((n - fndays) + 1);
	for (n = ndays; n->name; ++n)
		if (!strncasecmp(s, n->name, n->len))
			return ((n - ndays) + 1);
	for (p = days; *p; ++p)
		if (!strncasecmp(s, *p, 3))
			return ((p - days) + 1);
	return (0);
}

/* return offset for variable weekdays
 * -1 -> last weekday in month
 * +1 -> first weekday in month
 * ... etc ...
 */
int
getdayvar(s)
	register char *s;
{
	register int offset;


	offset = strlen(s);


	/* Sun+1 or Wednesday-2
	 *    ^              ^   */

	/* printf ("x: %s %s %d\n", s, s + offset - 2, offset); */
	switch(*(s + offset - 2)) {
	case '-':
	    return(-(atoi(s + offset - 1)));
	    break;
	case '+':
	    return(atoi(s + offset - 1));
	    break;
	}


	/*
	 * some aliases: last, first, second, third, fourth
	 */

	/* last */
	if      (offset > 4 && !strcasecmp(s + offset - 4, "last"))
	    return(-1);
	else if (offset > 5 && !strcasecmp(s + offset - 5, "first"))
	    return(+1);
	else if (offset > 6 && !strcasecmp(s + offset - 6, "second"))
	    return(+2);
	else if (offset > 5 && !strcasecmp(s + offset - 5, "third"))
	    return(+3);
	else if (offset > 6 && !strcasecmp(s + offset - 6, "fourth"))
	    return(+4);


	/* no offset detected */
	return(0);
}
