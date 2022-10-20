/*	$OpenBSD: mib.c,v 1.2 2020/05/19 13:41:01 martijn Exp $	*/

/*
 * Copyright (c) 2012 Joel Knight <joel@openbsd.org>
 * Copyright (c) 2007, 2008, 2012 Reyk Floeter <reyk@openbsd.org>
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


#include <sys/tree.h>
#include <sys/types.h>
#include <sys/queue.h>

#include "ber.h"
#include "mib.h"
#include "smi.h"

static struct oid mib_tree[] = MIB_TREE;
static struct textconv textconv_tree[] = TEXTCONV_TREE;

void
mib_init(void)
{
	smi_mibtree(mib_tree);
	smi_textconvtree(textconv_tree);
}
