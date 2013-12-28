/*	$OpenBSD: installboot.h,v 1.1 2013/12/27 13:52:40 jsing Exp $	*/
/*
 * Copyright (c) 2012, 2013 Joel Sing <jsing@openbsd.org>
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

extern int nowrite;
extern int stages;
extern int verbose;

extern char *stage1;
extern char *stage2;

#ifdef BOOTSTRAP
void	bootstrap(int, char *, char *);
#endif

void	md_init(void);
void	md_loadboot(void);
void	md_installboot(int, char *);

#ifdef SOFTRAID
void	sr_installboot(int, char *);
void	sr_install_bootblk(int, int, int);
void	sr_install_bootldr(int, char *);
#endif
