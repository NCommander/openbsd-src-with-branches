/*	$OpenBSD: disk.h,v 1.21 2015/12/12 02:49:50 krw Exp $	*/

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

#ifndef _DISK_H
#define _DISK_H

struct disk {
	char	 *name;
	int	  fd;
	uint32_t cylinders;
	uint32_t heads;
	uint32_t sectors;
	uint32_t size;
};

void  DISK_open(int);
int  DISK_printgeometry(char *);
char *DISK_readsector(off_t);
int DISK_writesector(char *, off_t);

extern struct disk disk;
extern struct disklabel dl;

#endif /* _DISK_H */
