/*	$OpenBSD: termcap.c,v 1.9 1997/12/16 04:11:59 millert Exp $	*/
/*	$NetBSD: termcap.c,v 1.7 1995/06/05 19:45:52 pk Exp $	*/

/*
 * Copyright (c) 1980, 1993
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
#if 0
static char sccsid[] = "@(#)termcap.c	8.1 (Berkeley) 6/4/93";
#else
static char rcsid[] = "$OpenBSD: termcap.c,v 1.9 1997/12/16 04:11:59 millert Exp $";
#endif
#endif /* not lint */

#define	PBUFSIZ		512	/* max length of filename path */
#define	PVECSIZ		32	/* max number of names in path */

#include <sys/param.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include "pathnames.h"

/*
 * termcap - routines for dealing with the terminal capability data base
 *
 * BUG:		Should use a "last" pointer in tbuf, so that searching
 *		for capabilities alphabetically would not be a n**2/2
 *		process when large numbers of capabilities are given.
 * Note:	If we add a last pointer now we will screw up the
 *		tc capability. We really should compile termcap.
 *
 * Essentially all the work here is scanning and decoding escapes
 * in string capabilities.  We don't use stdio because the editor
 * doesn't, and because living w/o it is not hard.
 */

static	char *tbuf;	/* termcap buffer */

/*
 * Get an entry for terminal name in buffer bp from the termcap file.
 */
int
tgetent(bp, name)
	char *bp, *name;
{
	register char *p;
	register char *cp;
	char  *dummy;
	char **fname;
	char  *home;
	int    i;
	char   pathbuf[MAXPATHLEN];	/* holds raw path of filenames */
	char  *pathvec[PVECSIZ];	/* to point to names in pathbuf */
	char **pvec;			/* holds usable tail of path vector */
	char  *termpath;

	fname = pathvec;
	pvec = pathvec;
	tbuf = bp;
	p = pathbuf;
	cp = getenv("TERMCAP");
	/*
	 * TERMCAP can have one of two things in it. It can be the name
	 * of a file to use instead of /usr/share/misc/termcap. In this
	 * case it better start with a "/". Or it can be an entry to
	 * use so we don't have to read the file. In this case it
	 * has to already have the newlines crunched out.  If TERMCAP
	 * does not hold a file name then a path of names is searched
	 * instead.  The path is found in the TERMPATH variable, or becomes
	 * "$HOME/.termcap /usr/share/misc/termcap" if no TERMPATH exists.
	 */
	if (!cp || *cp != '/') {	/* no TERMCAP or it holds an entry */
		if ((termpath = getenv("TERMPATH")) != NULL)
			strncpy(pathbuf, termpath, sizeof(pathbuf) - 1);
		else {
			if ((home = getenv("HOME")) != NULL) {
				/* set up default */
				/* $HOME first */
				strncpy(pathbuf, home, sizeof(pathbuf) - 1 -
				    strlen(_PATH_DEF) - 1);
				pathbuf[sizeof(pathbuf) - 1 -
				    strlen(_PATH_DEF) - 1] = '\0';
				p += strlen(pathbuf);	/* path, looking in */
				*p++ = '/';
			}	/* if no $HOME look in current directory */
			strncpy(p, _PATH_DEF, sizeof(pathbuf) -1 -
			    (p - pathbuf));
		}
	}
	else				/* user-defined name in TERMCAP */
		strncpy(pathbuf, cp, sizeof(pathbuf) - 1); /* still can be tokenized */
	pathbuf[sizeof(pathbuf) - 1] = '\0';

	*fname++ = pathbuf;	/* tokenize path into vector of names */
	while (*++p)
		if (*p == ' ' || *p == ':') {
			*p = '\0';
			while (*++p)
				if (*p != ' ' && *p != ':')
					break;
			if (*p == '\0')
				break;
			*fname++ = p;
			if (fname >= pathvec + PVECSIZ) {
				fname--;
				break;
			}
		}
	*fname = (char *) 0;			/* mark end of vector */
	if (cp && *cp && *cp != '/')
		if (cgetset(cp) < 0)
			return (-2);

	dummy = NULL;
	i = cgetent(&dummy, pathvec, name);
	
	if (i == 0 && bp != NULL) {
		strncpy(bp, dummy, 1023);
		bp[1023] = '\0';
		if ((cp = strrchr(bp, ':')) != NULL)
			if (cp[1] != '\0')
				cp[1] = '\0';
	}
	else if (i == 0 && bp == NULL)
		tbuf = dummy;
	else if (dummy != NULL)
		free(dummy);

	/* no tc reference loop return code in libterm XXX */
	if (i == -3)
		return (-1);
	return (i + 1);
}

/*
 * Return the (numeric) option id.
 * Numeric options look like
 *	li#80
 * i.e. the option string is separated from the numeric value by
 * a # character.  If the option is not found we return -1.
 * Note that we handle octal numbers beginning with 0.
 */
int
tgetnum(id)
	char *id;
{
	long num;

	if (cgetnum(tbuf, id, &num) == 0)
		return (num);
	else
		return (-1);
}

/*
 * Handle a flag option.
 * Flag options are given "naked", i.e. followed by a : or the end
 * of the buffer.  Return 1 if we find the option, or 0 if it is
 * not given.
 */
int
tgetflag(id)
	char *id;
{
	return (cgetcap(tbuf, id, ':') != NULL);
}

/*
 * Get a string valued option.
 * These are given as
 *	cl=^Z
 * Much decoding is done on the strings, and the strings are
 * placed in area, which is a ref parameter which is updated.
 * No checking on area overflow.
 */
char *
tgetstr(id, area)
	char *id, **area;
{
	char ids[3];
	char *s;
	int i;
	
	/*
	 * XXX
	 * This is for all the boneheaded programs that relied on tgetstr
	 * to look only at the first 2 characters of the string passed...
	 */
	*ids = *id;
	ids[1] = id[1];
	ids[2] = '\0';

	if ((i = cgetstr(tbuf, ids, &s)) < 0)
		return NULL;
	
	strcpy(*area, s);
	*area += i + 1;
	return (s);
}
