/*	$OpenBSD: abuf.h,v 1.19 2009/10/09 16:49:48 ratchov Exp $	*/
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

#define XRUN_IGNORE	0	/* on xrun silently insert/discard samples */
#define XRUN_SYNC	1	/* catchup to sync to the mix/sub */
#define XRUN_ERROR	2	/* xruns are errors, eof/hup buffer */
#define MIDI_MSGMAX	16	/* max size of MIDI messaage */

struct aproc;
struct aparams;

struct abuf {
	LIST_ENTRY(abuf) ient;	/* reader's list of inputs entry */
	LIST_ENTRY(abuf) oent;	/* writer's list of outputs entry */

	/*
	 * fifo parameters
	 */
	unsigned bpf;		/* bytes per frame */
	unsigned cmin, cmax;	/* channel range of this buf */
	unsigned start;		/* offset where data starts */
	unsigned used;		/* valid data */
	unsigned len;		/* size of the ring */
	unsigned abspos;	/* frame number of the start position */
	unsigned silence;	/* silence to insert on next write */
	unsigned drop;		/* bytes to drop on next read */
	struct aproc *rproc;	/* reader */
	struct aproc *wproc;	/* writer */
	struct abuf *duplex;	/* link to buffer of the other direction */
	unsigned inuse;		/* in abuf_{flush,fill,run}() */
	unsigned tickets;	/* max data to (if throttling) */

	/*
	 * Misc reader aproc-specific per-buffer parameters.
	 */
	union {
		struct {
			int weight;	/* dynamic range */	
			int maxweight;	/* max dynamic range allowed */
			unsigned vol;	/* volume within the dynamic range */
			unsigned done;	/* bytes ready */
			unsigned xrun;	/* underrun policy */
		} mix;
		struct {
			unsigned st;	/* MIDI running status */
			unsigned used;	/* bytes used from ``msg'' */
			unsigned idx;	/* actual MIDI message size */
			unsigned len;	/* MIDI message length */
			unsigned char msg[MIDI_MSGMAX];
		} midi;
	} r;

	/*
	 * Misc reader aproc-specific per-buffer parameters.
	 */
	union {
		struct {
			unsigned todo;	/* bytes to process */
		} mix;
		struct {
			unsigned done;	/* bytes copied */
			unsigned xrun;	/* overrun policy */
		} sub;
	} w;
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
 * the buffer is empty and has no writer anymore
 */
#define ABUF_EOF(b) (!ABUF_ROK(b) && (b)->wproc == NULL)

/*
 * the buffer has no reader anymore, note that it's not
 * enough the buffer to be disconnected, because it can
 * be not yet connected buffer (eg. socket play buffer)
 */
#define ABUF_HUP(b) (!ABUF_WOK(b) && (b)->rproc == NULL)

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

struct abuf *abuf_new(unsigned, struct aparams *);
void abuf_del(struct abuf *);
void abuf_dbg(struct abuf *);
void abuf_clear(struct abuf *);
unsigned char *abuf_rgetblk(struct abuf *, unsigned *, unsigned);
unsigned char *abuf_wgetblk(struct abuf *, unsigned *, unsigned);
void abuf_rdiscard(struct abuf *, unsigned);
void abuf_wcommit(struct abuf *, unsigned);
int abuf_fill(struct abuf *);
int abuf_flush(struct abuf *);
void abuf_eof(struct abuf *);
void abuf_hup(struct abuf *);
void abuf_run(struct abuf *);
void abuf_ipos(struct abuf *, int);
void abuf_opos(struct abuf *, int);

#endif /* !defined(ABUF_H) */
