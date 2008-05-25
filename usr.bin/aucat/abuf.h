/*	$OpenBSD: abuf.h,v 1.1 2008/05/23 07:15:46 ratchov Exp $	*/
/*
 * Copyright (c) 2008 Alexandre Ratchov <alex@caoua.org>
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
#ifndef ABUF_H
#define ABUF_H

#include <sys/queue.h>

struct abuf;
struct aproc;

struct abuf {
	/*
	 * misc aproc-specific per-buffer parameters
	 */
	int mixvol;		/* input gain */
	unsigned mixdone;	/* input of mixer */
	unsigned mixtodo;	/* output of mixer */
	unsigned subdone;	/* output if sub */
	LIST_ENTRY(abuf) ient;	/* for mix inputs list */
	LIST_ENTRY(abuf) oent;	/* for sub outputs list */

	/*
	 * fifo parameters
	 */
	unsigned bpf;		/* bytes per frame */
	unsigned start;		/* offset where data starts */
	unsigned used;		/* valid data */
	unsigned len;		/* size of the ring */
	struct aproc *rproc;	/* reader */
	struct aproc *wproc;	/* writer */
	unsigned char *data;	/* pointer to actual data (immediately following) */
};

/*
 * the buffer contains at least one frame. This macro should
 * be used to check if the buffer can be flushed
 */
#define ABUF_ROK(b) ((b)->used >= (b)->bpf)

/*
 * there's room for at least one frame
 */
#define ABUF_WOK(b) ((b)->len - (b)->used >= (b)->bpf)

/*
 * the buffer is empty and has no more writer
 */
#define ABUF_EOF(b) (!ABUF_ROK(b) && (b)->wproc == NULL)

/*
 * similar to !ABUF_WOK, but is used for file i/o, where
 * operation may not involve an integer number of frames
 */
#define ABUF_FULL(b) ((b)->used == (b)->len)

/*
 * same as !ABUF_ROK, but used for files, where
 * operations are byte orientated, not frame-oriented
 */
#define ABUF_EMPTY(b) ((b)->used == 0)

struct abuf *abuf_new(unsigned, unsigned);
void abuf_del(struct abuf *);
unsigned char *abuf_rgetblk(struct abuf *, unsigned *, unsigned);
unsigned char *abuf_wgetblk(struct abuf *, unsigned *, unsigned);
void abuf_fill(struct abuf *);
void abuf_flush(struct abuf *);
void abuf_eof(struct abuf *);
void abuf_hup(struct abuf *);
void abuf_run(struct abuf *);

#endif /* !defined(ABUF_H) */
