/*	$OpenBSD: roff_term.c,v 1.3 2017/05/05 13:17:04 schwarze Exp $ */
/*
 * Copyright (c) 2010, 2014, 2015, 2017 Ingo Schwarze <schwarze@openbsd.org>
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
#include <stddef.h>

#include "roff.h"
#include "out.h"
#include "term.h"

#define	ROFF_TERM_ARGS struct termp *p, const struct roff_node *n

typedef	void	(*roff_term_pre_fp)(ROFF_TERM_ARGS);

static	void	  roff_term_pre_br(ROFF_TERM_ARGS);
static	void	  roff_term_pre_ft(ROFF_TERM_ARGS);
static	void	  roff_term_pre_ll(ROFF_TERM_ARGS);
static	void	  roff_term_pre_sp(ROFF_TERM_ARGS);

static	const roff_term_pre_fp roff_term_pre_acts[ROFF_MAX] = {
	roff_term_pre_br,  /* br */
	roff_term_pre_ft,  /* ft */
	roff_term_pre_ll,  /* ft */
	roff_term_pre_sp,  /* br */
};


void
roff_term_pre(struct termp *p, const struct roff_node *n)
{
	assert(n->tok < ROFF_MAX);
	(*roff_term_pre_acts[n->tok])(p, n);
}

static void
roff_term_pre_br(ROFF_TERM_ARGS)
{
	term_newln(p);
	if (p->flags & TERMP_BRIND) {
		p->offset = p->rmargin;
		p->rmargin = p->maxrmargin;
		p->flags &= ~(TERMP_NOBREAK | TERMP_BRIND);
	}
}

static void
roff_term_pre_ft(ROFF_TERM_ARGS)
{
	switch (*n->child->string) {
	case '4':
	case '3':
	case 'B':
		term_fontrepl(p, TERMFONT_BOLD);
		break;
	case '2':
	case 'I':
		term_fontrepl(p, TERMFONT_UNDER);
		break;
	case 'P':
		term_fontlast(p);
		break;
	case '1':
	case 'C':
	case 'R':
		term_fontrepl(p, TERMFONT_NONE);
		break;
	default:
		break;
	}
}

static void
roff_term_pre_ll(ROFF_TERM_ARGS)
{
	term_setwidth(p, n->child != NULL ? n->child->string : NULL);
}

static void
roff_term_pre_sp(ROFF_TERM_ARGS)
{
	struct roffsu	 su;
	int		 len;

	if (n->child != NULL) {
		if (a2roffsu(n->child->string, &su, SCALE_VS) == 0)
			su.scale = 1.0;
		len = term_vspan(p, &su);
	} else
		len = 1;

	if (len < 0)
		p->skipvsp -= len;
	else
		while (len--)
			term_vspace(p);

	roff_term_pre_br(p, n);
}
