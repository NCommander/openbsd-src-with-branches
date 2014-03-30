/*	$OpenBSD$	*/
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
#ifndef APROC_H
#define APROC_H

#include <sys/queue.h>

#include "aparams.h"
#include "file.h"

struct abuf;
struct aproc;
struct file;

struct aproc_ops {
	/*
	 * Name of the ops structure, ie type of the unit.
	 */
	char *name;

	/*
	 * The state of the given input abuf changed (eg. an input block
	 * is ready for processing). This function must get the block
	 * from the input, process it and remove it from the buffer.
	 *
	 * Processing the block will result in a change of the state of
	 * OTHER buffers that are attached to the aproc (eg. the output
	 * buffer was filled), thus this routine MUST notify ALL aproc
	 * structures that are waiting on it; most of the time this
	 * means just calling abuf_flush() on the output buffer.
	 */
	int (*in)(struct aproc *, struct abuf *);

	/*
	 * The state of the given output abuf changed (eg. space for a
	 * new output block was made available) so processing can
	 * continue.  This function must process more input in order to
	 * fill the output block.
	 *
	 * Producing a block will result in the change of the state of
	 * OTHER buffers that are attached to the aproc, thus this
	 * routine MUST notify ALL aproc structures that are waiting on
	 * it; most of the time this means calling abuf_fill() on the
	 * source buffers.
	 *
	 * Before filling input buffers (using abuf_fill()), this
	 * routine must ALWAYS check for eof condition, and if needed,
	 * handle it appropriately and call abuf_hup() to free the input
	 * buffer.
	 */
	int (*out)(struct aproc *, struct abuf *);

	/*
	 * The input buffer is empty and we can no more receive data
	 * from it. The buffer will be destroyed as soon as this call
	 * returns so the abuf pointer will stop being valid after this
	 * call returns. There's no need to drain the buffer because the
	 * in() call-back was just called before.
	 *
	 * If this call reads and/or writes data on other buffers,
	 * abuf_flush() and abuf_fill() must be called appropriately.
	 */
	void (*eof)(struct aproc *, struct abuf *);

	/*
	 * The output buffer can no more accept data (it should be
	 * considered as full). After this function returns, it will be
	 * destroyed and the "abuf" pointer will be no more valid.
	 */
	void (*hup)(struct aproc *, struct abuf *);

	/*
	 * A new input was connected.
	 */
	void (*newin)(struct aproc *, struct abuf *);

	/*
	 * A new output was connected
	 */
	void (*newout)(struct aproc *, struct abuf *);

	/*
	 * Real-time record position changed (for input buffer),
	 * by the given amount of _frames_.
	 */
	void (*ipos)(struct aproc *, struct abuf *, int);

	/*
	 * Real-time play position changed (for output buffer),
	 * by the given amount of _frames_.
	 */
	void (*opos)(struct aproc *, struct abuf *, int);

	/*
	 * Destroy the aproc, called just before to free the
	 * aproc structure.
	 */
	void (*done)(struct aproc *);
};

/*
 * The aproc structure represents a simple audio processing unit; they are
 * interconnected by abuf structures and form a kind of "circuit". The circuit
 * cannot have loops.
 */
struct aproc {
	char *name;				/* for debug purposes */
	struct aproc_ops *ops;			/* call-backs */
	LIST_HEAD(, abuf) ins;			/* list of inputs */
	LIST_HEAD(, abuf) outs;			/* list of outputs */
	unsigned int refs;			/* extern references */
#define APROC_ZOMB	1			/* destroyed but not freed */
#define APROC_QUIT	2			/* try to terminate if unused */
#define APROC_DROP	4			/* xrun if capable */
	unsigned int flags;					
	union {					/* follow type-specific data */
		struct {			/* file/device io */
			struct file *file;	/* file to read/write */
			unsigned int partial;	/* bytes of partial frame */
		} io;
		struct {
			unsigned int idle;	/* frames since idleing */
			unsigned int round;	/* block size, for xruns */
			int lat;		/* current latency */
			int maxlat;		/* max latency allowed */
			unsigned int abspos;	/* frames produced */
			struct aproc *mon;	/* snoop output */
			unsigned int autovol;	/* adjust volume dynamically */
			int master;		/* master attenuation */
		} mix;
		struct {
			unsigned int idle;	/* frames since idleing */
			unsigned int round;	/* block size, for xruns */
			int lat;		/* current latency */
			int maxlat;		/* max latency allowed */
			unsigned int abspos;	/* frames consumed */
		} sub;
		struct {
			int delta;		/* time position */
			unsigned int bufsz;	/* buffer size (latency) */
			unsigned int pending;	/* uncommited samples */
		} mon;
		struct {
#define RESAMP_NCTX	2
			unsigned int ctx_start;
			adata_t ctx[NCHAN_MAX * RESAMP_NCTX];
			unsigned int iblksz, oblksz;
			int diff;
			int idelta, odelta;	/* remainder of resamp_xpos */
		} resamp;
		struct {
			int bfirst;		/* bytes to skip at startup */
			unsigned int bps;	/* bytes per sample */
			unsigned int shift;	/* shift to get 32bit MSB */
			int sigbit;		/* sign bits to XOR */
			int bnext;		/* to reach the next byte */
			int snext;		/* to reach the next sample */
		} conv;
		struct {
			struct dev *dev;	/* controlled device */
			struct timo timo;	/* timout for throtteling */
			unsigned int fps;	/* MTC frames per second */
#define MTC_FPS_24	0
#define MTC_FPS_25	1
#define MTC_FPS_30	3
			unsigned int fps_id;	/* one of above */
			unsigned int hr;	/* MTC hours */
			unsigned int min;	/* MTC minutes */
			unsigned int sec;	/* MTC seconds */
			unsigned int fr;	/* MTC frames */
			unsigned int qfr;	/* MTC quarter frames */
			int delta;		/* rel. to the last MTC tick */
		} midi;
	} u;
};

/*
 * Check if the given pointer is a valid aproc structure.
 *
 * aproc structures are not free()'d immediately, because
 * there may be pointers to them, instead the APROC_ZOMB flag
 * is set which means that they should not be used. When
 * aprocs reference counter reaches zero, they are actually
 * freed
 */
#define APROC_OK(p) ((p) && !((p)->flags & APROC_ZOMB))


struct aproc *aproc_new(struct aproc_ops *, char *);
void aproc_del(struct aproc *);
void aproc_dbg(struct aproc *);
void aproc_setin(struct aproc *, struct abuf *);
void aproc_setout(struct aproc *, struct abuf *);
int aproc_inuse(struct aproc *);
int aproc_depend(struct aproc *, struct aproc *);

void aproc_ipos(struct aproc *, struct abuf *, int);
void aproc_opos(struct aproc *, struct abuf *, int);

struct aproc *rfile_new(struct file *);
struct aproc *wfile_new(struct file *);
struct aproc *mix_new(char *, int, unsigned int, unsigned int, unsigned int);
struct aproc *sub_new(char *, int, unsigned int);
struct aproc *resamp_new(char *, unsigned int, unsigned int);
struct aproc *enc_new(char *, struct aparams *);
struct aproc *dec_new(char *, struct aparams *);
struct aproc *join_new(char *);
struct aproc *mon_new(char *, unsigned int);

int rfile_in(struct aproc *, struct abuf *);
int rfile_out(struct aproc *, struct abuf *);
void rfile_eof(struct aproc *, struct abuf *);
void rfile_hup(struct aproc *, struct abuf *);
void rfile_done(struct aproc *);
int rfile_do(struct aproc *, unsigned int, unsigned int *);

int wfile_in(struct aproc *, struct abuf *);
int wfile_out(struct aproc *, struct abuf *);
void wfile_eof(struct aproc *, struct abuf *);
void wfile_hup(struct aproc *, struct abuf *);
void wfile_done(struct aproc *);
int wfile_do(struct aproc *, unsigned int, unsigned int *);

void mix_setmaster(struct aproc *);
void mix_clear(struct aproc *);
void mix_prime(struct aproc *);
void mix_quit(struct aproc *);
void mix_drop(struct abuf *, int);
void sub_silence(struct abuf *, int);
void sub_clear(struct aproc *);
void mon_snoop(struct aproc *, struct abuf *, unsigned int, unsigned int);
void mon_clear(struct aproc *);

#endif /* !defined(APROC_H) */
