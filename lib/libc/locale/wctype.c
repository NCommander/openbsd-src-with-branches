/*	$OpenBSD: iswctype.c,v 1.7 2017/09/05 03:16:13 schwarze Exp $ */
/*	$NetBSD: iswctype.c,v 1.15 2005/02/09 21:35:46 kleink Exp $	*/

/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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

#include <wchar.h>
#include <wctype.h>
#include <string.h>
#include "rune.h"
#include "runetype.h"

static struct _WCTypeEntry wctype_entries[_WCTYPE_NINDEXES] =
{
	{ "alnum", _CTYPE_A|_CTYPE_D },
	{ "alpha", _CTYPE_A },
	{ "blank", _CTYPE_B },
	{ "cntrl", _CTYPE_C },
	{ "digit", _CTYPE_D },
	{ "graph", _CTYPE_G },
	{ "lower", _CTYPE_L },
	{ "print", _CTYPE_R },
	{ "punct", _CTYPE_P },
	{ "space", _CTYPE_S },
	{ "upper", _CTYPE_U },
	{ "xdigit", _CTYPE_X },
};

wctype_t
wctype(const char *property)
{
	int i;

	for (i = 0; i < _WCTYPE_NINDEXES; i++)
		if (strcmp(wctype_entries[i].te_name, property) == 0)
			return &wctype_entries[i];
	return NULL;
}
DEF_STRONG(wctype);

wctype_t
wctype_l(const char *property, locale_t locale __attribute__((__unused__)))
{
	return wctype(property);
}
