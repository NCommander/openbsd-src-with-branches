/*	$NetBSD: xargs.c,v 1.7 1994/11/14 06:51:41 jtc Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * John B. Roll Jr.
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
static char copyright[] =
"@(#) Copyright (c) 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)xargs.c	8.1 (Berkeley) 6/6/93";
#endif
static char rcsid[] = "$NetBSD: xargs.c,v 1.7 1994/11/14 06:51:41 jtc Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <locale.h>
#include <err.h>
#include "pathnames.h"

int tflag, rval;
int zflag;

void run __P((char **));
void usage __P((void));

int
main(argc, argv)
	int argc;
	char **argv;
{
	register int ch;
	register char *p, *bbp, *ebp, **bxp, **exp, **xp;
	int cnt, indouble, insingle, nargs, nflag, nline, xflag;
	char **av, *argp;

	setlocale(LC_ALL, "");

	/*
	 * POSIX.2 limits the exec line length to ARG_MAX - 2K.  Running that
	 * caused some E2BIG errors, so it was changed to ARG_MAX - 4K.  Given
	 * that the smallest argument is 2 bytes in length, this means that
	 * the number of arguments is limited to:
	 *
	 *	 (ARG_MAX - 4K - LENGTH(utility + arguments)) / 2.
	 *
	 * We arbitrarily limit the number of arguments to 5000.  This is
	 * allowed by POSIX.2 as long as the resulting minimum exec line is
	 * at least LINE_MAX.  Realloc'ing as necessary is possible, but
	 * probably not worthwhile.
	 */
	nargs = 5000;
	nline = ARG_MAX - 4 * 1024;
	nflag = xflag = 0;
	while ((ch = getopt(argc, argv, "0n:s:tx")) != EOF)
		switch(ch) {
		case 'n':
			nflag = 1;
			if ((nargs = atoi(optarg)) <= 0)
				errx(1, "illegal argument count");
			break;
		case 's':
			nline = atoi(optarg);
			break;
		case 't':
			tflag = 1;
			break;
		case 'x':
			xflag = 1;
			break;
		case '0':
			zflag = 1;
			break;
		case '?':
		default:
			usage();
	}
	argc -= optind;
	argv += optind;

	if (xflag && !nflag)
		usage();

	/*
	 * Allocate pointers for the utility name, the utility arguments,
	 * the maximum arguments to be read from stdin and the trailing
	 * NULL.
	 */
	if (!(av = bxp =
	    malloc((u_int)(1 + argc + nargs + 1) * sizeof(char **))))
		err(1, NULL);

	/*
	 * Use the user's name for the utility as argv[0], just like the
	 * shell.  Echo is the default.  Set up pointers for the user's
	 * arguments.
	 */
	if (!*argv)
		cnt = strlen(*bxp++ = _PATH_ECHO);
	else {
		cnt = 0;
		do {
			cnt += strlen(*bxp++ = *argv) + 1;
		} while (*++argv);
	}

	/*
	 * Set up begin/end/traversing pointers into the array.  The -n
	 * count doesn't include the trailing NULL pointer, so the malloc
	 * added in an extra slot.
	 */
	exp = (xp = bxp) + nargs;

	/*
	 * Allocate buffer space for the arguments read from stdin and the
	 * trailing NULL.  Buffer space is defined as the default or specified
	 * space, minus the length of the utility name and arguments.  Set up
	 * begin/end/traversing pointers into the array.  The -s count does
	 * include the trailing NULL, so the malloc didn't add in an extra
	 * slot.
	 */
	nline -= cnt;
	if (nline <= 0)
		errx(1, "insufficient space for command");

	if (!(bbp = malloc((u_int)nline + 1)))
		err(1, NULL);
	ebp = (argp = p = bbp) + nline - 1;

	for (insingle = indouble = 0;;)
		switch(ch = getchar()) {
		case EOF:
			/* No arguments since last exec. */
			if (p == bbp)
				exit(rval);

			/* Nothing since end of last argument. */
			if (argp == p) {
				*xp = NULL;
				run(av);
				exit(rval);
			}
			goto arg1;
		case ' ':
		case '\t':
			/* Quotes escape tabs and spaces. */
			if (insingle || indouble || zflag)
				goto addch;
			goto arg2;
		case '\0':
			if (zflag)
				goto arg2;
			goto addch;
		case '\n':
			if (zflag)
				goto addch;

			/* Empty lines are skipped. */
			if (argp == p)
				continue;

			/* Quotes do not escape newlines. */
arg1:			if (insingle || indouble)
				 errx(1, "unterminated quote");

arg2:			*p = '\0';
			*xp++ = argp;

			/*
			 * If max'd out on args or buffer, or reached EOF,
			 * run the command.  If xflag and max'd out on buffer
			 * but not on args, object.
			 */
			if (xp == exp || p == ebp || ch == EOF) {
				if (xflag && xp != exp && p == ebp)
					errx(1, "insufficient space for arguments");
				*xp = NULL;
				run(av);
				if (ch == EOF)
					exit(rval);
				p = bbp;
				xp = bxp;
			} else
				++p;
			argp = p;
			break;
		case '\'':
			if (indouble || zflag)
				goto addch;
			insingle = !insingle;
			break;
		case '"':
			if (insingle || zflag)
				goto addch;
			indouble = !indouble;
			break;
		case '\\':
			if (zflag)
				goto addch;
			/* Backslash escapes anything, is escaped by quotes. */
			if (!insingle && !indouble && (ch = getchar()) == EOF)
				errx(1, "backslash at EOF");
			/* FALLTHROUGH */
		default:
addch:			if (p < ebp) {
				*p++ = ch;
				break;
			}

			/* If only one argument, not enough buffer space. */
			if (bxp == xp)
				errx(1, "insufficient space for argument");
			/* Didn't hit argument limit, so if xflag object. */
			if (xflag)
				errx(1, "insufficient space for arguments");

			*xp = NULL;
			run(av);
			xp = bxp;
			cnt = ebp - argp;
			bcopy(argp, bbp, cnt);
			p = (argp = bbp) + cnt;
			*p++ = ch;
			break;
		}
	/* NOTREACHED */
}

void
run(argv)
	char **argv;
{
	volatile int noinvoke;
	register char **p;
	pid_t pid;
	int status;

	if (tflag) {
		(void)fprintf(stderr, "%s", *argv);
		for (p = argv + 1; *p; ++p)
			(void)fprintf(stderr, " %s", *p);
		(void)fprintf(stderr, "\n");
		(void)fflush(stderr);
	}
	noinvoke = 0;
	switch(pid = vfork()) {
	case -1:
		err(1, "vfork");
	case 0:
		execvp(argv[0], argv);
		noinvoke = (errno == ENOENT) ? 127 : 126;
		warn("%s", argv[0]);;
		_exit(1);
	}
	pid = waitpid(pid, &status, 0);
	if (pid == -1)
		err(1, "waitpid");

	/*
	 * If we couldn't invoke the utility or the utility didn't exit
	 * properly, quit with 127 or 126 respectively.
	 */
	if (noinvoke)
		exit(noinvoke);

	/*
	 * According to POSIX, we have to exit if the utility exits with
	 * a 255 status, or is interrupted by a signal.   xargs is allowed
	 * to return any exit status between 1 and 125 in these cases, but
	 * we'll use 124 and 125, the same values used by GNU xargs.
	 */
	if (WIFEXITED(status)) {
		if (WEXITSTATUS (status) == 255) {
			warnx ("%s exited with status 255", argv[0]);
			exit(124);
		} else if (WEXITSTATUS (status) != 0) {
			rval = 123;
		}
	} else if (WIFSIGNALED (status)) {
		warnx ("%s terminated by signal %d", argv[0], WTERMSIG(status));
		exit(125);
	}
}

void
usage()
{
	(void)fprintf(stderr,
"usage: xargs [-0] [-t] [-n number [-x]] [-s size] [utility [argument ...]]\n");
	exit(1);
}
