/*	$OpenBSD: io.c,v 1.4 1998/03/30 06:59:27 deraadt Exp $	*/

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
static char rcsid[] = "$OpenBSD: io.c,v 1.4 1998/03/30 06:59:27 deraadt Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <locale.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tzfile.h>
#include <unistd.h>

#include "pathnames.h"
#include "calendar.h"


char *calendarFile = "calendar";  /* default calendar file */
char *calendarHome = ".calendar"; /* HOME */
char *calendarNoMail = "nomail";  /* don't sent mail if this file exist */

struct fixs neaster, npaskha;

struct iovec header[] = {
	{"From: ", 6},
	{NULL, 0},
	{" (Reminder Service)\nTo: ", 24},
	{NULL, 0},
	{"\nSubject: ", 10},
	{NULL, 0},
	{"'s Calendar\nPrecedence: bulk\n\n",  30},
};


void
cal()
{
	register int printing;
	register char *p;
	FILE *fp;
	int ch, l;
	int var;
	char buf[2048 + 1];
	struct event *events, *cur_evt, *tmp;
	struct match *m;

	events = NULL;
	cur_evt = NULL;
	if ((fp = opencal()) == NULL)
		return;
	for (printing = 0; fgets(buf, sizeof(buf), stdin) != NULL;) {
		if ((p = strchr(buf, '\n')) != NULL)
			*p = '\0';
		else
			while ((ch = getchar()) != '\n' && ch != EOF);
		for (l = strlen(buf);
		     l > 0 && isspace(buf[l - 1]);
		     l--)
			;
		buf[l] = '\0';
		if (buf[0] == '\0')
			continue;
		if (strncmp(buf, "LANG=", 5) == 0) {
			(void) setlocale(LC_ALL, buf + 5);
			setnnames();
			continue;
		}
		if (strncasecmp(buf, "Easter=", 7) == 0 && buf[7]) {
			if (neaster.name != NULL)
				free(neaster.name);
			if ((neaster.name = strdup(buf + 7)) == NULL)
				errx(1, "cannot allocate memory");
			neaster.len = strlen(buf + 7);
			continue;
		}
		if (strncasecmp(buf, "Paskha=", 7) == 0 && buf[7]) {
			if (npaskha.name != NULL)
				free(npaskha.name);
			if ((npaskha.name = strdup(buf + 7)) == NULL)
				errx(1, "cannot allocate memory");
			npaskha.len = strlen(buf + 7);
			continue;
		}
		if (buf[0] != '\t') {
			printing = (m = isnow(buf)) ? 1 : 0;
			if ((p = strchr(buf, '\t')) == NULL)
				continue;
			/* Need the following to catch hardwired "variable"
			 * dates */
			if (p > buf && p[-1] == '*')
				var = 1;
			else
				var = 0;
			if (printing) {
				struct tm tm;
				struct match *foo;
				char *dsc;
				
				dsc = NULL;
				while (m) {
				cur_evt = (struct event *) malloc(sizeof(struct event));
				if (cur_evt == NULL)
					errx(1, "cannot allocate memory");

				tm.tm_sec = 0;  /* unused */
				tm.tm_min = 0;  /* unused */
				tm.tm_hour = 12; /* unused */
				tm.tm_wday = 0; /* unused */
				tm.tm_mon = m->month - 1;
				tm.tm_mday = m->day;
				tm.tm_year = m->year;
				tm.tm_isdst = tp->tm_isdst; /* unused */
				tm.tm_gmtoff = tp->tm_gmtoff; /* unused */
				tm.tm_zone = tp->tm_zone; /* unused */
				(void)strftime(cur_evt->print_date,
				    sizeof(cur_evt->print_date) - 1,
				/*    "%a %b %d", &tm);  Skip weekdays */
				    "%b %d", &tm);
				strcat(cur_evt->print_date,
				    (var + m->var) ? "*" : " ");
				cur_evt->when = mktime(&tm);
				if (dsc)
					cur_evt->desc = dsc;
				else {
					if ((cur_evt->desc = strdup(p)) == NULL)
						errx(1, "cannot allocate memory");
					dsc = cur_evt->desc;
				}
				insert(&events, cur_evt);
				foo = m;
				m = m->next;
				free(foo);
				}
			}
		}
		else if (printing) {
			if ((cur_evt->desc = realloc(cur_evt->desc,
			    (2 + strlen(cur_evt->desc) + strlen(buf)))) == NULL)
				errx(1, "cannot allocate memory");
			strcat(cur_evt->desc, "\n");
			strcat(cur_evt->desc, buf);
		}
	}
	tmp = events;
	while (tmp) {
		(void)fprintf(fp, "%s%s\n", tmp->print_date, tmp->desc);
		/* Can't free descriptions since they may be shared */
		(void)realloc(tmp->desc, 0);
		events = tmp;
		tmp = tmp->next;
		free(events);
	}
	closecal(fp);
}

int
getfield(p, endp, flags)
	char *p, **endp;
	int *flags;
{
	int val, var;
	char *start, savech;

	for (; !isdigit(*p) && !isalpha(*p) && *p != '*'; ++p)
		;
	if (*p == '*') {			/* `*' is current month */
		*flags |= F_ISMONTH;
		*endp = p+1;
		return (tp->tm_mon + 1);
	}
	if (isdigit(*p)) {
		val = strtol(p, &p, 10);	/* if 0, it's failure */
		for (; !isdigit(*p) && !isalpha(*p) && *p != '*'; ++p)
			;
		*endp = p;
		return (val);
	}
	for (start = p; isalpha(*++p);)
		;

	/* Sunday-1 */
	if (*p == '+' || *p == '-')
	    for(; isdigit(*++p);)
		;

	savech = *p;
	*p = '\0';

	/* Month */
	if ((val = getmonth(start)) != 0)
		*flags |= F_ISMONTH;

	/* Day */
	else if ((val = getday(start)) != 0) {
	    *flags |= F_ISDAY;

	    /* variable weekday */
	    if ((var = getdayvar(start)) != 0) {
		if (var <=5 && var >= -4)
		    val += var * 10;
#ifdef DEBUG
		printf("var: %d\n", var);
#endif
	    }
	}

	/* Easter */
	else if ((val = geteaster(start, tp->tm_year + TM_YEAR_BASE)) != 0)
	    *flags |= F_EASTER;

	/* Paskha */
	else if ((val = getpaskha(start, tp->tm_year + TM_YEAR_BASE)) != 0)
	    *flags |= F_EASTER;

	/* undefined rest */
	else {
		*p = savech;
		return (0);
	}
	for (*p = savech; !isdigit(*p) && !isalpha(*p) && *p != '*'; ++p)
		;
	*endp = p;
	return (val);
}

char path[MAXPATHLEN];

FILE *
opencal()
{
	int fd, pdes[2];
	struct stat sbuf;

	/* open up calendar file as stdin */
	if (!freopen(calendarFile, "r", stdin)) {
		if (doall) {
		    if (chdir(calendarHome) != 0)
			return (NULL);
		    if (stat(calendarNoMail, &sbuf) == 0)
		        return (NULL);
		    if (!freopen(calendarFile, "r", stdin))
		        return (NULL);
		} else {
		        chdir(getenv("HOME"));
			if (!(chdir(calendarHome) == 0 &&
			      freopen(calendarFile, "r", stdin)))
				errx(1, "no calendar file: ``%s'' or ``~/%s/%s",
				    calendarFile, calendarHome, calendarFile);
		}
	}
	if (pipe(pdes) < 0)
		return (NULL);
	switch (vfork()) {
	case -1:			/* error */
		(void)close(pdes[0]);
		(void)close(pdes[1]);
		return (NULL);
	case 0:
		/* child -- stdin already setup, set stdout to pipe input */
		if (pdes[1] != STDOUT_FILENO) {
			(void)dup2(pdes[1], STDOUT_FILENO);
			(void)close(pdes[1]);
		}
		(void)close(pdes[0]);
		(void)setuid(geteuid());
		(void)setgid(getegid());
		execl(_PATH_CPP, "cpp", "-P", "-I.", _PATH_INCLUDE, NULL);
		warn(_PATH_CPP);
		_exit(1);
	}
	/* parent -- set stdin to pipe output */
	(void)dup2(pdes[0], STDIN_FILENO);
	(void)close(pdes[0]);
	(void)close(pdes[1]);

	/* not reading all calendar files, just set output to stdout */
	if (!doall)
		return (stdout);

	/* set output to a temporary file, so if no output don't send mail */
	(void)snprintf(path, sizeof(path), "%s/_calXXXXXX", _PATH_TMP);
	if ((fd = mkstemp(path)) < 0)
		return (NULL);
	return (fdopen(fd, "w+"));
}

void
closecal(fp)
	FILE *fp;
{
	struct stat sbuf;
	int nread, pdes[2], status;
	char buf[1024];

	if (!doall)
		return;

	(void)rewind(fp);
	if (fstat(fileno(fp), &sbuf) || !sbuf.st_size)
		goto done;
	if (pipe(pdes) < 0)
		goto done;
	switch (vfork()) {
	case -1:			/* error */
		(void)close(pdes[0]);
		(void)close(pdes[1]);
		goto done;
	case 0:
		/* child -- set stdin to pipe output */
		if (pdes[0] != STDIN_FILENO) {
			(void)dup2(pdes[0], STDIN_FILENO);
			(void)close(pdes[0]);
		}
		(void)close(pdes[1]);
		(void)setuid(geteuid());
		(void)setgid(getegid());
		execl(_PATH_SENDMAIL, "sendmail", "-i", "-t", "-F",
		    "\"Reminder Service\"", "-f", "root", NULL);
		warn(_PATH_SENDMAIL);
		_exit(1);
	}
	/* parent -- write to pipe input */
	(void)close(pdes[0]);

	header[1].iov_base = header[3].iov_base = pw->pw_name;
	header[1].iov_len = header[3].iov_len = strlen(pw->pw_name);
	writev(pdes[1], header, 7);
	while ((nread = read(fileno(fp), buf, sizeof(buf))) > 0)
		(void)write(pdes[1], buf, nread);
	(void)close(pdes[1]);
done:	(void)fclose(fp);
	(void)unlink(path);
	while (wait(&status) >= 0);
}


void
insert(head, cur_evt)
	struct event **head;
	struct event *cur_evt;
{
	struct event *tmp, *tmp2;

	if (*head) {
		/* Insert this one in order */
		tmp = *head;
		tmp2 = NULL;
		while (tmp->next &&
		    tmp->when <= cur_evt->when) {
			tmp2 = tmp;
			tmp = tmp->next;
		}
		if (tmp->when > cur_evt->when) {
			cur_evt->next = tmp;
			if (tmp2)
				tmp2->next = cur_evt;
			else
				*head = cur_evt;
		} else {
			cur_evt->next = tmp->next;
			tmp->next = cur_evt;
		}
	} else {
		*head = cur_evt;
		cur_evt->next = NULL;
	}
}
