/*	$OpenBSD: fdcache.c,v 1.9 2003/05/12 17:35:44 henning Exp $ */

/*
 * Copyright (c) 2002, 2003 Henning Brauer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

struct fdcache {
    char *fname;
    int  fd;
    struct fdcache *next;
};

struct fdcache	*fdc;

int
fdcache_open(char *fn, int flags, mode_t mode)
{
    struct fdcache *fdcp = NULL, *tmp = NULL;

    for (fdcp = fdc; fdcp && strcmp(fn, fdcp->fname); fdcp = fdcp->next);
	/* nothing */

    if (fdcp == NULL) {
	/* need to open */
	if ((tmp = calloc(1, sizeof(struct fdcache))) == NULL)
	    err(1, "calloc");
	if ((tmp->fname = strdup(fn)) == NULL)
	    err(1, "strdup");
	if ((tmp->fd = open(fn, flags, mode)) < 0)
	    err(1, "Cannot open %s", tmp->fname);
	tmp->next = fdc;
	fdc = tmp;
	return(fdc->fd);
    } else
	return(fdcp->fd);	/* fd cached */
}

void
fdcache_closeall(void)
{
    struct fdcache *fdcp = NULL, *tmp = NULL;

    for (fdcp = fdc; fdcp != NULL; ) {
	tmp = fdcp;
	fdcp = tmp->next;
	if (tmp->fd > 0)
	    close(tmp->fd);
	free(tmp->fname);
	free(tmp);
    }
}

