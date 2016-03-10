/*	$OpenBSD: part.h,v 1.20 2015/10/26 15:08:26 krw Exp $	*/

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

#ifndef _PART_H
#define _PART_H

struct prt {
	u_int64_t bs;
	u_int64_t ns;
	u_int32_t shead, scyl, ssect;
	u_int32_t ehead, ecyl, esect;
	unsigned char flag;
	unsigned char id;
};

void	PRT_printall(void);
const char *PRT_ascii_id(int);
void PRT_parse(struct dos_partition *, off_t, off_t,
    struct prt *);
void PRT_make(struct prt *, off_t, off_t, struct dos_partition *);
void PRT_print(int, struct prt *, char *);
char *PRT_uuid_to_typename(struct uuid *);
int PRT_uuid_to_type(struct uuid *);
struct uuid *PRT_type_to_uuid(int);

/* This does CHS -> bs/ns */
void PRT_fix_BN(struct prt *, int);

/* This does bs/ns -> CHS */
void PRT_fix_CHS(struct prt *);

#endif /* _PART_H */
