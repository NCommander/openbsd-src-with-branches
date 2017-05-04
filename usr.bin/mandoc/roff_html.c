/*	$OpenBSD$ */
/*
 * Copyright (c) 2017 Ingo Schwarze <schwarze@openbsd.org>
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

#include "roff.h"
#include "out.h"
#include "html.h"

#define	ROFF_HTML_ARGS struct html *h, const struct roff_node *n

typedef	void	(*roff_html_pre_fp)(ROFF_HTML_ARGS);

static	void	  roff_html_pre_br(ROFF_HTML_ARGS);

static	const roff_html_pre_fp roff_html_pre_acts[ROFF_MAX] = {
	roff_html_pre_br,  /* br */
};


void
roff_html_pre(struct html *h, const struct roff_node *n)
{
	assert(n->tok < ROFF_MAX);
	(*roff_html_pre_acts[n->tok])(h, n);
}

static void
roff_html_pre_br(ROFF_HTML_ARGS)
{
	print_otag(h, TAG_DIV, "");
	print_text(h, "\\~");  /* So the div isn't empty. */
}
