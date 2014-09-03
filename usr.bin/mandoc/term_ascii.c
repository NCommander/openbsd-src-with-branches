/*	$OpenBSD$ */
/*
 * Copyright (c) 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 * Copyright (c) 2014 Ingo Schwarze <schwarze@openbsd.org>
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

#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <wchar.h>

#include "mandoc.h"
#include "mandoc_aux.h"
#include "out.h"
#include "term.h"
#include "main.h"

static	struct termp	 *ascii_init(enum termenc, char *);
static	double		  ascii_hspan(const struct termp *,
				const struct roffsu *);
static	size_t		  ascii_width(const struct termp *, int);
static	void		  ascii_advance(struct termp *, size_t);
static	void		  ascii_begin(struct termp *);
static	void		  ascii_end(struct termp *);
static	void		  ascii_endline(struct termp *);
static	void		  ascii_letter(struct termp *, int);
static	void		  ascii_setwidth(struct termp *, int, size_t);

static	void		  locale_advance(struct termp *, size_t);
static	void		  locale_endline(struct termp *);
static	void		  locale_letter(struct termp *, int);
static	size_t		  locale_width(const struct termp *, int);


static struct termp *
ascii_init(enum termenc enc, char *outopts)
{
	const char	*toks[5];
	char		*v;
	struct termp	*p;

	p = mandoc_calloc(1, sizeof(struct termp));

	p->tabwidth = 5;
	p->defrmargin = p->lastrmargin = 78;

	p->begin = ascii_begin;
	p->end = ascii_end;
	p->hspan = ascii_hspan;
	p->type = TERMTYPE_CHAR;

	p->enc = TERMENC_ASCII;
	p->advance = ascii_advance;
	p->endline = ascii_endline;
	p->letter = ascii_letter;
	p->setwidth = ascii_setwidth;
	p->width = ascii_width;

	if (TERMENC_ASCII != enc) {
		v = TERMENC_LOCALE == enc ?
		    setlocale(LC_ALL, "") :
		    setlocale(LC_CTYPE, "en_US.UTF-8");
		if (NULL != v && MB_CUR_MAX > 1) {
			p->enc = enc;
			p->advance = locale_advance;
			p->endline = locale_endline;
			p->letter = locale_letter;
			p->width = locale_width;
		}
	}

	toks[0] = "indent";
	toks[1] = "width";
	toks[2] = "mdoc";
	toks[3] = "synopsis";
	toks[4] = NULL;

	while (outopts && *outopts)
		switch (getsubopt(&outopts, UNCONST(toks), &v)) {
		case 0:
			p->defindent = (size_t)atoi(v);
			break;
		case 1:
			p->defrmargin = (size_t)atoi(v);
			break;
		case 2:
			/*
			 * Temporary, undocumented mode
			 * to imitate mdoc(7) output style.
			 */
			p->mdocstyle = 1;
			p->defindent = 5;
			break;
		case 3:
			p->synopsisonly = 1;
			break;
		default:
			break;
		}

	/* Enforce a lower boundary. */
	if (p->defrmargin < 58)
		p->defrmargin = 58;

	return(p);
}

void *
ascii_alloc(char *outopts)
{

	return(ascii_init(TERMENC_ASCII, outopts));
}

void *
utf8_alloc(char *outopts)
{

	return(ascii_init(TERMENC_UTF8, outopts));
}

void *
locale_alloc(char *outopts)
{

	return(ascii_init(TERMENC_LOCALE, outopts));
}

static void
ascii_setwidth(struct termp *p, int iop, size_t width)
{

	p->rmargin = p->defrmargin;
	if (0 < iop)
		p->defrmargin += width;
	else if (0 > iop)
		p->defrmargin -= width;
	else
		p->defrmargin = width ? width : p->lastrmargin;
	p->lastrmargin = p->rmargin;
	p->rmargin = p->maxrmargin = p->defrmargin;
}

static size_t
ascii_width(const struct termp *p, int c)
{

	return(1);
}

void
ascii_free(void *arg)
{

	term_free((struct termp *)arg);
}

static void
ascii_letter(struct termp *p, int c)
{

	putchar(c);
}

static void
ascii_begin(struct termp *p)
{

	(*p->headf)(p, p->argf);
}

static void
ascii_end(struct termp *p)
{

	(*p->footf)(p, p->argf);
}

static void
ascii_endline(struct termp *p)
{

	putchar('\n');
}

static void
ascii_advance(struct termp *p, size_t len)
{
	size_t		i;

	for (i = 0; i < len; i++)
		putchar(' ');
}

static double
ascii_hspan(const struct termp *p, const struct roffsu *su)
{
	double		 r;

	/*
	 * Approximate based on character width.
	 * None of these will be actually correct given that an inch on
	 * the screen depends on character size, terminal, etc., etc.
	 */
	switch (su->unit) {
	case SCALE_BU:
		r = su->scale * 10.0 / 240.0;
		break;
	case SCALE_CM:
		r = su->scale * 10.0 / 2.54;
		break;
	case SCALE_FS:
		r = su->scale * 2730.666;
		break;
	case SCALE_IN:
		r = su->scale * 10.0;
		break;
	case SCALE_MM:
		r = su->scale / 100.0;
		break;
	case SCALE_PC:
		r = su->scale * 10.0 / 6.0;
		break;
	case SCALE_PT:
		r = su->scale * 10.0 / 72.0;
		break;
	case SCALE_VS:
		r = su->scale * 2.0 - 1.0;
		break;
	case SCALE_EN:
		/* FALLTHROUGH */
	case SCALE_EM:
		r = su->scale;
		break;
	default:
		abort();
		/* NOTREACHED */
	}

	return(r);
}

static size_t
locale_width(const struct termp *p, int c)
{
	int		rc;

	if (c == ASCII_NBRSP)
		c = ' ';
	rc = wcwidth(c);
	if (rc < 0)
		rc = 0;
	return(rc);
}

static void
locale_advance(struct termp *p, size_t len)
{
	size_t		i;

	for (i = 0; i < len; i++)
		putwchar(L' ');
}

static void
locale_endline(struct termp *p)
{

	putwchar(L'\n');
}

static void
locale_letter(struct termp *p, int c)
{

	putwchar(c);
}
