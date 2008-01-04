/*	$OpenBSD: misc.c,v 1.8 2003/06/12 20:58:10 deraadt Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Hugh Smith at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
/*static char sccsid[] = "from: @(#)misc.c	5.2 (Berkeley) 2/26/91";*/
static char rcsid[] = "$OpenBSD: misc.c,v 1.8 2003/06/12 20:58:10 deraadt Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pathnames.h"
#include "extern.h"

char *tname = "temporary file";

int
tmp(void)
{
	static char *envtmp;
	sigset_t set, oset;
	static int first;
	int fd;
	char path[MAXPATHLEN];

	if (!first) {
		envtmp = getenv("TMPDIR");
		first = 1;
	}

	if (envtmp)
		(void)snprintf(path, sizeof(path), "%s/%s", envtmp,
		    _NAME_RANTMP);
	else
		strlcpy(path, _PATH_RANTMP, sizeof(path));

	sigemptyset(&set);
	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGQUIT);
	sigaddset(&set, SIGTERM);
	(void)sigprocmask(SIG_BLOCK, &set, &oset);
	if ((fd = mkstemp(path)) == -1)
		error(tname);
   	(void)unlink(path);
	(void)sigprocmask(SIG_SETMASK, &oset, (sigset_t *)NULL);
	return(fd);
}

void *
emalloc(size_t len)
{
	void *p;

	if (!(p = malloc(len)))
		error(archive);
	return(p);
}

const char *
rname(const char *path)
{
	const char *ind;

	return((ind = strrchr(path, '/')) ? ind + 1 : path);
}

void
badfmt(void)
{
	errno = EFTYPE;
	error(archive);
}

void
error(const char *name)
{

	err(1, "%s", name);
}
