/*	$Id: mandoc.c,v 1.14 2010/06/26 17:56:43 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010 Kristaps Dzonsons <kristaps@bsd.lv>
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
 */
#include <sys/types.h>

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "mandoc.h"
#include "libmandoc.h"

static	int	 a2time(time_t *, const char *, const char *);
static	int	 spec_norm(char *, int);


/*
 * "Normalise" a special string by converting its ASCII_HYPH entries
 * into actual hyphens.
 */
static int
spec_norm(char *p, int sz)
{
	int		 i;

	for (i = 0; i < sz; i++)
		if (ASCII_HYPH == p[i])
			p[i] = '-';

	return(sz);
}


int
mandoc_special(char *p)
{
	int		 terminator;	/* Terminator for \s. */
	int		 lim;		/* Limit for N in \s. */
	int		 c, i;
	char		*sv;
	
	sv = p;

	if ('\\' != *p++)
		return(spec_norm(sv, 0));

	switch (*p) {
	case ('\''):
		/* FALLTHROUGH */
	case ('`'):
		/* FALLTHROUGH */
	case ('q'):
		/* FALLTHROUGH */
	case (ASCII_HYPH):
		/* FALLTHROUGH */
	case ('-'):
		/* FALLTHROUGH */
	case ('~'):
		/* FALLTHROUGH */
	case ('^'):
		/* FALLTHROUGH */
	case ('%'):
		/* FALLTHROUGH */
	case ('0'):
		/* FALLTHROUGH */
	case (' '):
		/* FALLTHROUGH */
	case ('}'):
		/* FALLTHROUGH */
	case ('|'):
		/* FALLTHROUGH */
	case ('&'):
		/* FALLTHROUGH */
	case ('.'):
		/* FALLTHROUGH */
	case (':'):
		/* FALLTHROUGH */
	case ('c'):
		/* FALLTHROUGH */
	case ('e'):
		return(spec_norm(sv, 2));
	case ('s'):
		if ('\0' == *++p)
			return(spec_norm(sv, 2));

		c = 2;
		terminator = 0;
		lim = 1;

		if (*p == '\'') {
			lim = 0;
			terminator = 1;
			++p;
			++c;
		} else if (*p == '[') {
			lim = 0;
			terminator = 2;
			++p;
			++c;
		} else if (*p == '(') {
			lim = 2;
			terminator = 3;
			++p;
			++c;
		}

		if (*p == '+' || *p == '-') {
			++p;
			++c;
		}

		if (*p == '\'') {
			if (terminator)
				return(spec_norm(sv, 0));
			lim = 0;
			terminator = 1;
			++p;
			++c;
		} else if (*p == '[') {
			if (terminator)
				return(spec_norm(sv, 0));
			lim = 0;
			terminator = 2;
			++p;
			++c;
		} else if (*p == '(') {
			if (terminator)
				return(spec_norm(sv, 0));
			lim = 2;
			terminator = 3;
			++p;
			++c;
		}

		/* TODO: needs to handle floating point. */

		if ( ! isdigit((u_char)*p))
			return(spec_norm(sv, 0));

		for (i = 0; isdigit((u_char)*p); i++) {
			if (lim && i >= lim)
				break;
			++p;
			++c;
		}

		if (terminator && terminator < 3) {
			if (1 == terminator && *p != '\'')
				return(spec_norm(sv, 0));
			if (2 == terminator && *p != ']')
				return(spec_norm(sv, 0));
			++p;
			++c;
		}

		return(spec_norm(sv, c));
	case ('f'):
		/* FALLTHROUGH */
	case ('F'):
		/* FALLTHROUGH */
	case ('*'):
		if ('\0' == *++p || isspace((u_char)*p))
			return(spec_norm(sv, 0));
		switch (*p) {
		case ('('):
			if ('\0' == *++p || isspace((u_char)*p))
				return(spec_norm(sv, 0));
			return(spec_norm(sv, 4));
		case ('['):
			for (c = 3, p++; *p && ']' != *p; p++, c++)
				if (isspace((u_char)*p))
					break;
			return(spec_norm(sv, *p == ']' ? c : 0));
		default:
			break;
		}
		return(spec_norm(sv, 3));
	case ('('):
		if ('\0' == *++p || isspace((u_char)*p))
			return(spec_norm(sv, 0));
		if ('\0' == *++p || isspace((u_char)*p))
			return(spec_norm(sv, 0));
		return(spec_norm(sv, 4));
	case ('['):
		break;
	default:
		return(spec_norm(sv, 0));
	}

	for (c = 3, p++; *p && ']' != *p; p++, c++)
		if (isspace((u_char)*p))
			break;

	return(spec_norm(sv, *p == ']' ? c : 0));
}


void *
mandoc_calloc(size_t num, size_t size)
{
	void		*ptr;

	ptr = calloc(num, size);
	if (NULL == ptr) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	return(ptr);
}


void *
mandoc_malloc(size_t size)
{
	void		*ptr;

	ptr = malloc(size);
	if (NULL == ptr) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	return(ptr);
}


void *
mandoc_realloc(void *ptr, size_t size)
{

	ptr = realloc(ptr, size);
	if (NULL == ptr) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	return(ptr);
}


char *
mandoc_strdup(const char *ptr)
{
	char		*p;

	p = strdup(ptr);
	if (NULL == p) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	return(p);
}


static int
a2time(time_t *t, const char *fmt, const char *p)
{
	struct tm	 tm;
	char		*pp;

	memset(&tm, 0, sizeof(struct tm));

	pp = strptime(p, fmt, &tm);
	if (NULL != pp && '\0' == *pp) {
		*t = mktime(&tm);
		return(1);
	}

	return(0);
}


/*
 * Convert from a manual date string (see mdoc(7) and man(7)) into a
 * date according to the stipulated date type.
 */
time_t
mandoc_a2time(int flags, const char *p)
{
	time_t		 t;

	if (MTIME_MDOCDATE & flags) {
		if (0 == strcmp(p, "$" "Mdocdate$"))
			return(time(NULL));
		if (a2time(&t, "$" "Mdocdate: %b %d %Y $", p))
			return(t);
	}

	if (MTIME_CANONICAL & flags || MTIME_REDUCED & flags) 
		if (a2time(&t, "%b %d, %Y", p))
			return(t);

	if (MTIME_ISO_8601 & flags) 
		if (a2time(&t, "%Y-%m-%d", p))
			return(t);

	if (MTIME_REDUCED & flags) {
		if (a2time(&t, "%d, %Y", p))
			return(t);
		if (a2time(&t, "%Y", p))
			return(t);
	}

	return(0);
}


int
mandoc_eos(const char *p, size_t sz, int enclosed)
{
	const char *q;
	int found = 0;

	if (0 == sz)
		return(0);

	/*
	 * End-of-sentence recognition must include situations where
	 * some symbols, such as `)', allow prior EOS punctuation to
	 * propogate outward.
	 */

	for (q = p + sz - 1; q >= p; q--) {
		switch (*q) {
		case ('\"'):
			/* FALLTHROUGH */
		case ('\''):
			/* FALLTHROUGH */
		case (']'):
			/* FALLTHROUGH */
		case (')'):
			if (0 == found)
				enclosed = 1;
			break;
		case ('.'):
			/* FALLTHROUGH */
		case ('!'):
			/* FALLTHROUGH */
		case ('?'):
			found = 1;
			break;
		default:
			return(found && (!enclosed || isalnum(*q)));
		}
	}

	return(found && !enclosed);
}


int
mandoc_hyph(const char *start, const char *c)
{

	/*
	 * Choose whether to break at a hyphenated character.  We only
	 * do this if it's free-standing within a word.
	 */

	/* Skip first/last character of buffer. */
	if (c == start || '\0' == *(c + 1))
		return(0);
	/* Skip first/last character of word. */
	if ('\t' == *(c + 1) || '\t' == *(c - 1))
		return(0);
	if (' ' == *(c + 1) || ' ' == *(c - 1))
		return(0);
	/* Skip double invocations. */
	if ('-' == *(c + 1) || '-' == *(c - 1))
		return(0);
	/* Skip escapes. */
	if ('\\' == *(c - 1))
		return(0);

	return(1);
}
