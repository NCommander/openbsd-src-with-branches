/*	$OpenBSD: gpt.h,v 1.6 2016/01/09 18:10:57 krw Exp $	*/
/*
 * Copyright (c) 2015 Markus Muller <mmu@grummel.net>
 * Copyright (c) 2015 Kenneth R Westerback <krw@openbsd.org>
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

void		GPT_get_gpt(int);
int		GPT_get_hdr(off_t);
int		GPT_get_partition_table(off_t);
int		GPT_get_lba_start(unsigned int);
int		GPT_get_lba_end(unsigned int);

int		GPT_init(void);
int		GPT_write(void);
void		GPT_print(char *, int);
void		GPT_print_part(int, char *, int);
void		GPT_print_parthdr(int);

extern struct gpt_header gh;
extern struct gpt_partition gp[NGPTPARTITIONS];
