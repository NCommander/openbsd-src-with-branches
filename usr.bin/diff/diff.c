/*	$OpenBSD: diff.c,v 1.36 2003/07/27 18:45:55 millert Exp $	*/

/*
 * Copyright (c) 2003 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#ifndef lint
static const char rcsid[] = "$OpenBSD: diff.c,v 1.36 2003/07/27 18:45:55 millert Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "diff.h"

int	 aflag, bflag, dflag, iflag, lflag, Nflag, Pflag, rflag;
int	 sflag, tflag, Tflag, wflag;
int	 format, context, status;
char	*start, *ifdefname, *diffargs, *label;
struct stat stb1, stb2;
struct excludes *excludes_list;

#define	OPTIONS	"abC:cdD:efhiL:lnNPqrS:sTtU:uwX:x:"
static struct option longopts[] = {
	{ "text",			no_argument,		0,	'a' },
	{ "ignore-space-change",	no_argument,		0,	'b' },
	{ "context",			optional_argument,	0,	'C' },
	{ "ifdef",			required_argument,	0,	'D' },
	{ "minimal",			no_argument,		0,	'd' },
	{ "ed",				no_argument,		0,	'e' },
	{ "forward-ed",			no_argument,		0,	'f' },
	{ "ignore-case",		no_argument,		0,	'i' },
	{ "paginate",			no_argument,		0,	'l' },
	{ "label",			required_argument,	0,	'L' },
	{ "new-file",			no_argument,		0,	'N' },
	{ "rcs",			no_argument,		0,	'n' },
	{ "unidirectional-new-file",	no_argument,		0,	'P' },
	{ "brief",			no_argument,		0,	'q' },
	{ "recursive",			no_argument,		0,	'r' },
	{ "report-identical-files",	no_argument,		0,	's' },
	{ "starting-file",		required_argument,	0,	'S' },
	{ "expand-tabs",		no_argument,		0,	't' },
	{ "intial-tab",			no_argument,		0,	'T' },
	{ "unified",			optional_argument,	0,	'U' },
	{ "ignore-all-space",		no_argument,		0,	'w' },
	{ "exclude",			required_argument,	0,	'x' },
	{ "exclude-from",		required_argument,	0,	'X' },
	{ NULL,				0,			0,	'\0'}
};

__dead void usage(void);
void push_excludes(char *);
void read_excludes_file(char *file);
void set_argstr(char **, char **);

int
main(int argc, char **argv)
{
	char *ep, **oargv;
	long  l;
	int   ch, gotstdin;

	oargv = argv;
	gotstdin = 0;

	while ((ch = getopt_long(argc, argv, OPTIONS, longopts, NULL)) != -1) {
		switch (ch) {
		case 'a':
			aflag = 1;
			break;
		case 'b':
			bflag = 1;
			break;
		case 'C':
		case 'c':
			format = D_CONTEXT;
			if (optarg != NULL) {
				l = strtol(optarg, &ep, 10);
				if (*ep != '\0' || l < 0 || l >= INT_MAX)
					usage();
				context = (int)l;
			} else
				context = 3;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'D':
			format = D_IFDEF;
			ifdefname = optarg;
			break;
		case 'e':
			format = D_EDIT;
			break;
		case 'f':
			format = D_REVERSE;
			break;
		case 'h':
			/* silently ignore for backwards compatibility */
			break;
		case 'i':
			iflag = 1;
			break;
		case 'L':
			label = optarg;
			break;
		case 'l':
			lflag = 1;
			signal(SIGPIPE, SIG_IGN);
			break;
		case 'N':
			Nflag = 1;
			break;
		case 'n':
			format = D_NREVERSE;
			break;
		case 'P':
			Pflag = 1;
			break;
		case 'r':
			rflag = 1;
			break;
		case 'q':
			format = D_BRIEF;
			break;
		case 'S':
			start = optarg;
			break;
		case 's':
			sflag = 1;
			break;
		case 'T':
			Tflag = 1;
			break;
		case 't':
			tflag = 1;
			break;
		case 'U':
		case 'u':
			format = D_UNIFIED;
			if (optarg != NULL) {
				l = strtol(optarg, &ep, 10);
				if (*ep != '\0' || l < 0 || l >= INT_MAX)
					usage();
				context = (int)l;
			} else
				context = 3;
			break;
		case 'w':
			wflag = 1;
			break;
		case 'X':
			read_excludes_file(optarg);
			break;
		case 'x':
			push_excludes(optarg);
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	/*
	 * Do sanity checks, fill in stb1 and stb2 and call the appropriate
	 * driver routine.  Both drivers use the contents of stb1 and stb2.
	 */
	if (argc != 2)
		usage();
	if (strcmp(argv[0], "-") == 0) {
		fstat(STDIN_FILENO, &stb1);
		gotstdin = 1;
	} else if (stat(argv[0], &stb1) != 0)
		err(2, "%s", argv[0]);
	if (strcmp(argv[1], "-") == 0) {
		fstat(STDIN_FILENO, &stb2);
		gotstdin = 1;
	} else if (stat(argv[1], &stb2) != 0)
		err(2, "%s", argv[1]);
	if (gotstdin && (S_ISDIR(stb1.st_mode) || S_ISDIR(stb2.st_mode)))
		errx(2, "can't compare - to a directory");
	set_argstr(oargv + 1, argv);
	if (S_ISDIR(stb1.st_mode) && S_ISDIR(stb2.st_mode)) {
		if (format == D_IFDEF)
			errx(2, "-D option not supported with directories");
		diffdir(argv[0], argv[1]);
	} else {
		if (S_ISDIR(stb1.st_mode)) {
			argv[0] = splice(argv[0], argv[1]);
			if (stat(argv[0], &stb1) < 0)
				err(2, "%s", argv[0]);
		}
		if (S_ISDIR(stb2.st_mode)) {
			argv[1] = splice(argv[1], argv[0]);
			if (stat(argv[1], &stb2) < 0)
				err(2, "%s", argv[1]);
		}
		print_status(diffreg(argv[0], argv[1], 0), argv[0], argv[1],
		    NULL);
	}
	exit(status);
}

void *
emalloc(size_t n)
{
	void *p;

	if ((p = malloc(n)) == NULL)
		err(2, NULL);
	return (p);
}

void *
erealloc(void *p, size_t n)
{
	void *q;

	if ((q = realloc(p, n)) == NULL)
		err(2, NULL);
	return (q);
}

int
easprintf(char **ret, const char *fmt, ...)
{
	int len;
	va_list ap;

	va_start(ap, fmt);
	len = vasprintf(ret, fmt, ap);
	va_end(ap);

	if (len == -1)
		err(2, NULL);
	return (len);
}

void
set_argstr(char **av, char **ave)
{
	size_t argsize;
	char **ap;

	argsize = 4 + *ave - *av + 1;
	diffargs = emalloc(argsize);
	strlcpy(diffargs, "diff", argsize);
	for (ap = av + 1; ap < ave; ap++) {
		if (strcmp(*ap, "--") != 0) {
			strlcat(diffargs, " ", argsize);
			strlcat(diffargs, *ap, argsize);
		}
	}
}

/*
 * Read in an excludes file and push each line.
 */
void
read_excludes_file(char *file)
{
	FILE *fp;
	char *buf, *pattern;
	size_t len;

	if (strcmp(file, "-") == 0)
		fp = stdin;
	else if ((fp = fopen(file, "r")) == NULL)
		err(2, "%s", file);
	while ((buf = fgetln(fp, &len)) != NULL) {
		if (buf[len - 1] == '\n')
			len--;
		pattern = emalloc(len + 1);
		memcpy(pattern, buf, len);
		pattern[len] = '\0';
		push_excludes(pattern);
	}
	if (strcmp(file, "-") != 0)
		fclose(fp);
}

/*
 * Push a pattern onto the excludes list.
 */
void
push_excludes(char *pattern)
{
	struct excludes *entry;

	entry = emalloc(sizeof(*entry));
	entry->pattern = pattern;
	entry->next = excludes_list;
	excludes_list = entry;
}

void
print_status(int val, char *path1, char *path2, char *entry)
{
	switch (val) {
	case D_ONLY:
		/* must strip off the trailing '/' */
		printf("Only in %.*s: %s\n", (int)(strlen(path1) - 1),
		    path1, entry);
		break;
	case D_COMMON:
		printf("Common subdirectories: %s%s and %s%s\n",
		    path1, entry ? entry : "", path2, entry ? entry : "");
		break;
	case D_BINARY:
		printf("Binary files %s%s and %s%s differ\n",
		    path1, entry ? entry : "", path2, entry ? entry : "");
		break;
	case D_DIFFER:
		if (format == D_BRIEF)
			printf("Files %s%s and %s%s differ\n",
			    path1, entry ? entry : "",
			    path2, entry ? entry : "");
		break;
	case D_SAME:
		if (sflag)
			printf("Files %s%s and %s%s are identical\n",
			    path1, entry ? entry : "",
			    path2, entry ? entry : "");
		break;
	case D_MISMATCH1:
		printf("File %s%s is a directory while file %s%s is a regular file\n",
		    path1, entry ? entry : "", path2, entry ? entry : "");
		break;
	case D_MISMATCH2:
		printf("File %s%s is a regular file while file %s%s is a directory\n",
		    path1, entry ? entry : "", path2, entry ? entry : "");
		break;
	}
}

__dead void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: diff [-bdilqtTw] [-c | -e | -f | -n | -u] [-L label] file1 file2\n"
	    "       diff [-bdilqtTw] [-L label] -C number file1 file2\n"
	    "       diff [-bdilqtw] -D string file1 file2\n"
	    "       diff [-bdilqtTw] [-L label] -U number file1 file2\n"
	    "       diff [-bdilNPqwtT] [-c | -e | -f | -n | -u ] [-L label] [-r] [-s] [-S name]\n"
	    "            [-X file] [-x pattern] dir1 dir2\n");

	exit(2);
}
