/*	$OpenBSD$	*/

/*
 * Copyright (c) 1997 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
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
 *	This product includes software developed by Todd C. Miller.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint                                                              
static char rcsid[] = "$OpenBSD: mktemp.c,v 1.2 1997/01/03 22:49:22 millert Exp $";
#endif /* not lint */                                                        

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char *__progname;

int which __P((char *, char *));
void usage __P((void));

/*
 * which(1) -- find an executable(s) in the user's path
 *
 * Return values:
 *	0 - all executables found
 *	1 - some found, some not
 *	2 - none found
 */

int
main(argc, argv)
	int argc;
	char **argv;
{
	char *path;
	int n, notfound = 0;

	(void)setlocale(LC_ALL, "");

	if (argc == 1)
		usage();

	if ((path = getenv("PATH")) == NULL)
		err(-1, "Can't get $PATH from environment");

	/* To make access(2) do what we want */
	if (setgid(getegid()))
		err(-1, "Can't set gid to %u", getegid());
	if (setuid(geteuid()))
		err(-1, "Can't set uid to %u", geteuid());

	for (n = 1; n < argc; n++)
		if (which(argv[n], path) == 0)
			notfound++;

	exit((notfound == 0) ? 0 : ((notfound == argc - 1) ? 2 : 1));
}

int
which(prog, path)
	char *prog;
	char *path;
{
	char *p, filename[MAXPATHLEN];
	int proglen, plen;
	struct stat sbuf;

	/* Special case if prog contains '/' */
	if (strchr(prog, '/')) {
		if ((stat(prog, &sbuf) == 0) && S_ISREG(sbuf.st_mode) &&
		    access(prog, X_OK) == 0) {
			(void)puts(prog);
			return(1);
		} else {
			(void)printf("%s: Command not found.\n", prog);
			return(0);
		}
	}

	if ((path = strdup(path)) == NULL)
		errx(1, "Can't allocate memory.");

	proglen = strlen(prog);
	while ((p = strsep(&path, ":")) != NULL) {
		if (*p == '\0')
			p = ".";

		plen = strlen(p);
		while (p[plen-1] == '/')
			p[--plen] = '\0';	/* strip trailing '/' */

		if (plen + 1 + proglen >= sizeof(filename)) {
			warnx("%s/%s: %s", p, prog, strerror(ENAMETOOLONG));
			return(0);
		}

		(void)strcpy(filename, p);
		filename[plen] = '/';
		(void)strcpy(filename + plen + 1, prog);
		if ((stat(filename, &sbuf) == 0) && S_ISREG(sbuf.st_mode) &&
		    access(filename, X_OK) == 0) {
			(void)puts(filename);
			return(1);
		}
	}
	(void)free(path);

	(void)printf("%s: Command not found.\n", prog);
	return(0);
}

void
usage()
{
	(void) fprintf(stderr, "Usage: %s [name ...]\n", __progname);
	exit(1);
}
