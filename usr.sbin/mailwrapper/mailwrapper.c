/*	$OpenBSD: mailwrapper.c,v 1.14 2003/03/08 23:19:32 millert Exp $	*/
/*	$NetBSD: mailwrapper.c,v 1.2 1999/02/20 22:10:07 thorpej Exp $	*/

/*
 * Copyright (c) 1998
 * 	Perry E. Metzger.  All rights reserved.
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
 *    must display the following acknowledgment:
 *	This product includes software developed for the NetBSD Project
 *	by Perry E. Metzger.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>
#include <util.h>

#define _PATH_MAILERCONF	"/etc/mailer.conf"
#define _PATH_DEFAULTMTA	"/usr/libexec/sendmail/sendmail"

struct arglist {
	size_t argc, maxc;
	char **argv;
};

int main(int, char *[], char *[]);

static void initarg(struct arglist *);
static void addarg(struct arglist *, const char *, int);

extern const char *__progname;	/* from crt0.o */

static void
initarg(struct arglist *al)
{
	al->argc = 0;
	al->maxc = 10;
	if ((al->argv = malloc(al->maxc * sizeof(char *))) == NULL)
		err(1, "malloc");
}

static void
addarg(struct arglist *al, const char *arg, int copy)
{
	char **argv2;

	if (al->argc == al->maxc) {
		al->maxc <<= 1;

		if ((argv2 = realloc(al->argv,
		    al->maxc * sizeof(char *))) == NULL) {
			if (al->argv)
				free(al->argv);
			al->argv = NULL;
			err(1, "realloc");
		} else {
			al->argv = argv2;
		}
	}
	if (copy) {
		if ((al->argv[al->argc++] = strdup(arg)) == NULL)
			err(1, "strdup");
	} else
		al->argv[al->argc++] = (char *)arg;
}

int
main(int argc, char *argv[], char *envp[])
{
	FILE *config;
	char *line, *cp, *from, *to, *ap;
	const char *progname;
	size_t len, lineno = 0;
	struct arglist al;

	/* change __progname to mailwrapper so we get sensible error messages */
	progname = __progname;
	__progname = "mailwrapper";

	initarg(&al);
	for (len = 0; len < argc; len++)
		addarg(&al, argv[len], 0);

	if ((config = fopen(_PATH_MAILERCONF, "r")) == NULL) {
		addarg(&al, NULL, 0);
		openlog(__progname, LOG_PID, LOG_MAIL);
		syslog(LOG_INFO, "cannot open %s, using %s as default MTA",
		    _PATH_MAILERCONF, _PATH_DEFAULTMTA);
		closelog();
		execve(_PATH_DEFAULTMTA, al.argv, envp);
		err(1, "cannot exec %s", _PATH_DEFAULTMTA);
		/*NOTREACHED*/
	}

	for (;;) {
		if ((line = fparseln(config, &len, &lineno, NULL, 0)) == NULL) {
			if (feof(config))
				errx(1, "no mapping in %s", _PATH_MAILERCONF);
			err(1, "fparseln");
		}

#define	WS	" \t\n"
		cp = line;

		cp += strspn(cp, WS);
		if (cp[0] == '\0') {
			/* empty line */
			free(line);
			continue;
		}

		if ((from = strsep(&cp, WS)) == NULL)
			goto parse_error;

		cp += strspn(cp, WS);

		if ((to = strsep(&cp, WS)) == NULL)
			goto parse_error;

		if (strcmp(from, progname) == 0) {
			for (ap = strsep(&cp, WS); ap != NULL;
			    ap = strsep(&cp, WS))
				if (*ap)
					addarg(&al, ap, 0);
			break;
		}

		free(line);
	}

	(void)fclose(config);

	addarg(&al, NULL, 0);

	execve(to, al.argv, envp);
	err(1, "cannot exec %s", to);
	/*NOTREACHED*/
parse_error:
	errx(1, "parse error in %s at line %lu",
	    _PATH_MAILERCONF, (u_long)lineno);
	/*NOTREACHED*/
}
