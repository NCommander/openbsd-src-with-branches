/*	$OpenBSD: hexdump.c,v 1.19 2015/10/09 01:37:07 deraadt Exp $	*/
/*	$NetBSD: hexdump.c,v 1.7 1997/10/19 02:34:06 lukem Exp $	*/

/*
 * Copyright (c) 1989, 1993
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

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "hexdump.h"

#define MINIMUM(a, b)	(((a) < (b)) ? (a) : (b))

FS *fshead;				/* head of format strings */
int blocksize;				/* data block size */
int exitval;				/* final exit value */
long length = -1;			/* max bytes to read */
char *iobuf;				/* stdio I/O buffer */
size_t iobufsiz;			/* size of stdio I/O buffer */

int
main(int argc, char *argv[])
{
	FS *tfs;
	char *p;

	if (pledge("stdio rpath", NULL) == -1)
		err(1, "pledge");

	if (!(p = strrchr(argv[0], 'o')) || strcmp(p, "od"))
		newsyntax(argc, &argv);
	else
		oldsyntax(argc, &argv);

	/* figure out the data block size */
	for (blocksize = 0, tfs = fshead; tfs; tfs = tfs->nextfs) {
		tfs->bcnt = size(tfs);
		if (blocksize < tfs->bcnt)
			blocksize = tfs->bcnt;
	}
	if (length != -1) {
		iobufsiz = MINIMUM(length, blocksize);
		if ((iobuf = malloc(iobufsiz)) == NULL)
			err(1, NULL);
	}
	/* rewrite the rules, do syntax checking */
	for (tfs = fshead; tfs; tfs = tfs->nextfs)
		rewrite(tfs);

	(void)next(argv);
	display();
	exit(exitval);
}
