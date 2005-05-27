/*	$OpenBSD: pfkey.h,v 1.1 2005/05/25 17:10:26 hshoexer Exp $	*/
/*
 * Copyright (c) 2004, 2005 Hans-Joerg Hoexer <hshoexer@openbsd.org>
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

#ifndef _PFKEY_H_
#define _PFKEY_H_

#define PFKEYV2_CHUNK sizeof(u_int64_t)

int	pfkey_parse(struct sadb_msg *, struct ipsec_rule *);
void	pfkey_print_sa(struct sadb_msg *, int);
int	pfkey_ipsec_establish(struct ipsec_rule *);
int	pfkey_ipsec_flush(void);
int	pfkey_init(void);

#endif	/* _PFKEY_H_ */
