/*-
 * Copyright (c) 1991, 1993
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

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: exec.c,v 1.8 1998/08/14 21:39:23 deraadt Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <paths.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

extern char **environ;

static char **
buildargv(ap, arg, envpp)
	va_list ap;
	const char *arg;
	char ***envpp;
{
	register char **argv, **nargv;
	register int memsize, off;

	argv = NULL;
	for (off = memsize = 0;; ++off) {
		if (off >= memsize) {
			memsize += 50;	/* Starts out at 0. */
			memsize *= 2;	/* Ramp up fast. */
			nargv = realloc(argv, memsize * sizeof(char *));
			if (nargv == NULL) {
				if (argv)
					free(argv);
				return (NULL);
			}
			argv = nargv;
			if (off == 0) {
				argv[0] = (char *)arg;
				off = 1;
			}
		}
		if (!(argv[off] = va_arg(ap, char *)))
			break;
	}
	/* Get environment pointer if user supposed to provide one. */
	if (envpp)
		*envpp = va_arg(ap, char **);
	return (argv);
}

int
#ifdef __STDC__
execl(const char *name, const char *arg, ...)
#else
execl(name, arg, va_alist)
	const char *name;
	const char *arg;
	va_dcl
#endif
{
	va_list ap;
	int sverrno;
	char **argv;

#ifdef __STDC__
	va_start(ap, arg);
#else
	va_start(ap);
#endif
	if ((argv = buildargv(ap, arg, NULL)))
		(void)execve(name, argv, environ);
	va_end(ap);
	sverrno = errno;
	free(argv);
	errno = sverrno;
	return (-1);
}

int
#ifdef __STDC__
execle(const char *name, const char *arg, ...)
#else
execle(name, arg, va_alist)
	const char *name;
	const char *arg;
	va_dcl
#endif
{
	va_list ap;
	char **argv, **envp;
	int i;

#ifdef __STDC__
	va_start(ap, arg);
#else
	va_start(ap);
#endif
	for (i = 1; va_arg(ap, char *) != NULL; i++)
		;
	va_end(ap);

	argv = alloca (i * sizeof (char *));

#if __STDC__
	va_start(ap, arg);
#else
	va_start(ap);
#endif
	argv[0] = (char *) arg;
	for (i = 1; (argv[i] = (char *) va_arg(ap, char *)) != NULL; i++)
		;
	envp = (char **) va_arg(ap, char **);
	va_end(ap);

	return execve(name, argv, envp);
}

int
#ifdef __STDC__
execlp(const char *name, const char *arg, ...)
#else
execlp(name, arg, va_alist)
	const char *name;
	const char *arg;
	va_dcl
#endif
{
	va_list ap;
	int sverrno;
	char **argv;

#ifdef __STDC__
	va_start(ap, arg);
#else
	va_start(ap);
#endif
	if ((argv = buildargv(ap, arg, NULL)))
		(void)execvp(name, argv);
	va_end(ap);
	sverrno = errno;
	free(argv);
	errno = sverrno;
	return (-1);
}

int
execv(name, argv)
	const char *name;
	char * const *argv;
{
	(void)execve(name, argv, environ);
	return (-1);
}

int
execvp(name, argv)
	const char *name;
	char * const *argv;
{
	char **memp;
	register int cnt, lp, ln;
	register char *p;
	int eacces = 0, etxtbsy = 0;
	char *bp, *cur, *path, buf[MAXPATHLEN];

	/*
	 * Do not allow null name
	 */
	if (name == NULL || *name == '\0') {
		errno = ENOENT;
		return (-1);
 	}

	/* If it's an absolute or relative path name, it's easy. */
	if (strchr(name, '/')) {
		bp = (char *)name;
		cur = path = NULL;
		goto retry;
	}
	bp = buf;

	/* Get the path we're searching. */
	if (!(path = getenv("PATH")))
		path = _PATH_DEFPATH;
	cur = path = strdup(path);

	while ((p = strsep(&cur, ":"))) {
		/*
		 * It's a SHELL path -- double, leading and trailing colons
		 * mean the current directory.
		 */
		if (!*p) {
			p = ".";
			lp = 1;
		} else
			lp = strlen(p);
		ln = strlen(name);

		/*
		 * If the path is too long complain.  This is a possible
		 * security issue; given a way to make the path too long
		 * the user may execute the wrong program.
		 */
		if (lp + ln + 2 > sizeof(buf)) {
			struct iovec iov[3];

			iov[0].iov_base = "execvp: ";
			iov[0].iov_len = 8;
			iov[1].iov_base = p;
			iov[1].iov_len = lp;
			iov[2].iov_base = ": path too long\n";
			iov[2].iov_len = 16;
			(void)writev(STDERR_FILENO, iov, 3);
			continue;
		}
		bcopy(p, buf, lp);
		buf[lp] = '/';
		bcopy(name, buf + lp + 1, ln);
		buf[lp + ln + 1] = '\0';

retry:		(void)execve(bp, argv, environ);
		switch(errno) {
		case EACCES:
			eacces = 1;
			break;
		case ENOENT:
			break;
		case ENOEXEC:
			for (cnt = 0; argv[cnt]; ++cnt)
				;
			memp = malloc((cnt + 2) * sizeof(char *));
			if (memp == NULL)
				goto done;
			memp[0] = "sh";
			memp[1] = bp;
			bcopy(argv + 1, memp + 2, cnt * sizeof(char *));
			(void)execve(_PATH_BSHELL, memp, environ);
			free(memp);
			goto done;
		case ETXTBSY:
			if (etxtbsy < 3)
				(void)sleep(++etxtbsy);
			goto retry;
		default:
			goto done;
		}
	}
	if (eacces)
		errno = EACCES;
	else if (!errno)
		errno = ENOENT;
done:	if (path)
		free(path);
	return (-1);
}
