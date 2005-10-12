/*	$OpenBSD: iso_cksum.c,v 1.1 2005/01/28 14:05:40 claudio Exp $ */

/*
 * Copyright (c) 2005 Claudio Jeker <claudio@openbsd.org>
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

#include "ospfd.h"
#include "log.h"

/* pos needs to be 0 for verify and 2 <= pos < len for calculation */
u_int16_t
iso_cksum(void *buf, u_int16_t len, u_int16_t pos)
{
	u_int8_t	*p = buf;
	int		 c0 = 0, c1 = 0;	/* counters */
	int		 r0, r1;		/* results */
	u_int16_t	 sop;

	sop = len - pos - 1;	/* pos is an offset (pos 2 is at len 3) */
	p += 2;
	len -= 2;
	while (len--) {
		c0 += *p++;
		c1 += c0;
		if ((len & 0xfff) == 0) {
			/* overflow protection */
			c0 %= 255;
			c1 %= 255;
		}
	}
	r0 = c0 = c0 % 255;
	r1 = c1 % 255;

	if (pos) {
		r0 = ((sop * r0 - r1)) % 255;
		if (r0 <= 0)
			r0 += 255;
		r1 = 510 - r0 - c0;
		if (r1 > 255)
			r1 -= 255;
	}

	return (r0 << 8 | r1);
}
