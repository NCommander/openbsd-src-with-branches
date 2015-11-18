/*	$OpenBSD: mbr.h,v 1.27 2015/11/13 02:27:17 krw Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
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

#ifndef _MBR_H
#define _MBR_H

struct mbr {
	off_t reloffset;
	off_t offset;
	unsigned char code[DOSPARTOFF];
	struct prt part[NDOSPART];
	u_int16_t signature;
};

extern struct mbr initial_mbr;

void MBR_print(struct mbr *, char *);
void MBR_parse(struct dos_mbr *, off_t, off_t, struct mbr *);
void MBR_make(struct mbr *, struct dos_mbr *);
void MBR_init(struct mbr *);
void MBR_init_GPT(struct mbr *);
int MBR_read(off_t, struct dos_mbr *);
int MBR_write(off_t, struct dos_mbr *);
void MBR_zapgpt(struct dos_mbr *, uint64_t);
void MBR_init_GPT(struct mbr *);
int MBR_protective_mbr(struct mbr *);

#endif /* _MBR_H */
