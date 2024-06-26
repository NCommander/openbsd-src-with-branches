/*	$Id: dbg.c,v 1.3 2016/09/01 00:35:21 florian Exp $ */
/*
 * Copyright (c) 2016 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <err.h>
#include <stdarg.h>
#include <stdlib.h>

#include "extern.h"

void
doddbg(const char *fmt, ...)
{
	va_list		 ap;

	if (verbose < 2)
		return;

	va_start(ap, fmt);
	vwarnx(fmt, ap);
	va_end(ap);
}

void
dodbg(const char *fmt, ...)
{
	va_list		 ap;

	if (!verbose)
		return;

	va_start(ap, fmt);
	vwarnx(fmt, ap);
	va_end(ap);
}
