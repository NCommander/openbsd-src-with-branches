/*	$OpenBSD: part.h,v 1.43 2023/03/31 13:11:41 krw Exp $	*/

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

struct chs {
	uint64_t	chs_cyl;
	uint32_t	chs_head;
	uint32_t	chs_sect;
};

struct prt {
	uint64_t	prt_bs;
	uint64_t	prt_ns;
	unsigned char	prt_flag;
	unsigned char	prt_id;
};

void		 PRT_print_mbrmenu(char *, size_t);
void		 PRT_print_gptmenu(char *, size_t);
void		 PRT_parse(const struct dos_partition *, const uint64_t,
    const uint64_t, struct prt *);
void		 PRT_make(const struct prt *,const uint64_t, const uint64_t,
    struct dos_partition *);
void		 PRT_print_part(const int, const struct prt *, const char *);
void		 PRT_print_parthdr(void);
const char	*PRT_uuid_to_desc(const struct uuid *);
int		 PRT_uuid_to_menuid(const struct uuid *);
const struct uuid *PRT_menuid_to_uuid(const int);
int		 PRT_protected_uuid(const struct uuid *);
int		 PRT_lba_to_chs(const struct prt*, struct chs *, struct chs*);
