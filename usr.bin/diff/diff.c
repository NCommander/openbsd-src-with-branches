/*	$OpenBSD: diff.c,v 1.3 2003/06/25 03:02:33 tedu Exp $	*/

/*
 * Copyright (C) Caldera International Inc.  2001-2002.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code and documentation must retain the above
 *    copyright notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed or owned by Caldera
 *	International, Inc.
 * 4. Neither the name of Caldera International, Inc. nor the names of other
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE LIABLE FOR ANY DIRECT,
 * INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <unistd.h>

#include "diff.h"
#include "pathnames.h"

#if 0
static char const sccsid[] = "@(#)diff.c 4.7 5/11/89";
#endif

/*
 * diff - driver and subroutines
 */

char diff[] = _PATH_DIFF;
char diffh[] = _PATH_DIFFH;
char pr[] = _PATH_PR;

static void noroom(void);

int
main(int argc, char **argv)
{
	char *argp;

	ifdef1 = "FILE1";
	ifdef2 = "FILE2";
	status = 2;
	diffargv = argv;
	argc--, argv++;
	while (argc > 2 && argv[0][0] == '-') {
		argp = &argv[0][1];
		argv++, argc--;
		while (*argp)
			switch (*argp++) {
#ifdef notdef
			case 'I':
				opt = D_IFDEF;
				wantelses = 0;
				continue;
			case 'E':
				opt = D_IFDEF;
				wantelses = 1;
				continue;
			case '1':
				opt = D_IFDEF;
				ifdef1 = argp;
				*--argp = 0;
				continue;
#endif
			case 'D':
				/* -Dfoo = -E -1 -2foo */
				wantelses = 1;
				ifdef1 = "";
				/* fall through */
#ifdef notdef
			case '2':
#endif
				opt = D_IFDEF;
				ifdef2 = argp;
				*--argp = 0;
				continue;
			case 'e':
				opt = D_EDIT;
				continue;
			case 'f':
				opt = D_REVERSE;
				continue;
			case 'n':
				opt = D_NREVERSE;
				continue;
			case 'b':
				bflag = 1;
				continue;
			case 'w':
				wflag = 1;
				continue;
			case 'i':
				iflag = 1;
				continue;
			case 't':
				tflag = 1;
				continue;
			case 'c':
				opt = D_CONTEXT;
				if (isdigit(*argp)) {
					context = atoi(argp);
					while (isdigit(*argp))
						argp++;
					if (*argp) {
						fprintf(stderr,
						    "diff: -c: bad count\n");
						done(0);
					}
					argp = "";
				} else
					context = 3;
				continue;
			case 'h':
				hflag++;
				continue;
			case 'S':
				if (*argp == 0) {
					fprintf(stderr, "diff: use -Sstart\n");
					done(0);
				}
				start = argp;
				*--argp = 0;	/* don't pass it on */
				continue;
			case 'r':
				rflag++;
				continue;
			case 's':
				sflag++;
				continue;
			case 'l':
				lflag++;
				continue;
			default:
				fprintf(stderr, "diff: -%s: unknown option\n",
				    --argp);
				done(0);
			}
	}
	if (argc != 2) {
		fprintf(stderr, "diff: two filename arguments required\n");
		done(0);
	}
	file1 = argv[0];
	file2 = argv[1];
	if (hflag && opt) {
		fprintf(stderr,
		    "diff: -h doesn't support -e, -f, -n, -c, or -I\n");
		done(0);
	}
	if (!strcmp(file1, "-"))
		stb1.st_mode = S_IFREG;
	else if (stat(file1, &stb1) < 0) {
		fprintf(stderr, "diff: ");
		perror(file1);
		done(0);
	}
	if (!strcmp(file2, "-"))
		stb2.st_mode = S_IFREG;
	else if (stat(file2, &stb2) < 0) {
		fprintf(stderr, "diff: ");
		perror(file2);
		done(0);
	}
	if ((stb1.st_mode & S_IFMT) == S_IFDIR &&
	    (stb2.st_mode & S_IFMT) == S_IFDIR) {
		diffdir(argv);
	} else
		diffreg();
	done(0);
	/* notreached */
	return (0);
}

int
min(int a, int b)
{

	return (a < b ? a : b);
}

int
max(int a, int b)
{

	return (a > b ? a : b);
}

void
done(int sig)
{
	if (tempfile)
		unlink(tempfile);
	if (sig)
		_exit(status);
	exit(status);
}

void
catchsig(int sigraised)
{
	/* print something? */
	done(0);
}

void *
talloc(size_t n)
{
	void *p;

	if ((p = malloc(n)) == NULL)
		noroom();
	return (p);
}

void *
ralloc(void *p, size_t n)
{
	void *q;

	if ((q = realloc(p, n)) == NULL)
		noroom();
	return (q);
}

static void
noroom(void)
{
	fprintf(stderr, "diff: files too big, try -h\n");
	done(0);
}
