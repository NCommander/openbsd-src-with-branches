/*	$OpenBSD: binary.c,v 1.10 2003/12/29 21:20:55 canacar Exp $	*/

/*-
 * Copyright (c) 1999 James Howard and Dag-Erling Co�dan Sm�rgrav
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <ctype.h>
#include <stdio.h>
#include <zlib.h>

#include "grep.h"

#define	isbinary(ch)	(!isprint((ch)) && !isspace((ch)) && (ch) != '\b')

int
bin_file(FILE *f)
{
	char		buf[BUFSIZ];
	int		i, m;
	int		ret = 0;

	if (fseek(f, 0L, SEEK_SET) == -1)
		return 0;

	if ((m = (int)fread(buf, 1, BUFSIZ, f)) == 0)
		return 0;

	for (i = 0; i < m; i++)
		if (isbinary(buf[i])) {
			ret = 1;
			break;
		}

	rewind(f);
	return ret;
}

#ifndef NOZ
int
gzbin_file(gzFile *f)
{
	char		buf[BUFSIZ];
	int		i, m;
	int		ret = 0;

	if (gzseek(f, 0L, SEEK_SET) == -1)
		return 0;

	if ((m = gzread(f, buf, BUFSIZ)) == 0)
		return 0;

	for (i = 0; i < m; i++)
		if (isbinary(buf[i])) {
			ret = 1;
			break;
		}

	gzrewind(f);
	return ret;
}
#endif

int
mmbin_file(mmf_t *f)
{
	int i;

	/* XXX knows too much about mmf internals */
	for (i = 0; i < BUFSIZ && i < f->len; i++)
		if (isbinary(f->base[i]))
			return 1;
	return 0;
}
