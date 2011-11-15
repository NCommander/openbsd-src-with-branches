/*	$OpenBSD: dev.h,v 1.30 2011/10/12 07:20:04 ratchov Exp $	*/
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
#ifndef DEV_H
#define DEV_H

#include "aparams.h"

struct aproc;
struct abuf;

struct dev {
	struct dev *next;

	/*
	 * desired parameters
	 */
	unsigned reqmode;			/* mode */
	struct aparams reqipar, reqopar;	/* parameters */
	unsigned reqbufsz;			/* buffer size */
	unsigned reqround;			/* block size */
	unsigned reqrate;			/* sample rate */
	unsigned hold;				/* hold the device open ? */
	unsigned autovol;			/* auto adjust playvol ? */
	unsigned autostart;			/* don't wait for MMC start */
	unsigned refcnt;			/* number of openers */
#define DEV_NMAX	16			/* max number of devices */
	unsigned num;				/* serial number */
#define DEV_CLOSED	0			/* closed */
#define DEV_INIT	1			/* stopped */
#define DEV_START	2			/* ready to start */
#define DEV_RUN		3			/* started */
	unsigned pstate;			/* on of DEV_xxx */
	char *path;				/* sio path */

	/*
	 * actual parameters and runtime state (i.e. once opened)
	 */
	unsigned mode;				/* bitmap of MODE_xxx */
	unsigned bufsz, round, rate;
	struct aparams ipar, opar;
	struct aproc *mix, *sub, *submon;
	struct aproc *rec, *play, *mon;
	struct aproc *midi;
	struct devctl {
		struct devctl *next;
		unsigned mode;
		char *path;
	} *ctl_list;
};

extern struct dev *dev_list;

void dev_dbg(struct dev *);
int  dev_init(struct dev *);
int  dev_run(struct dev *);
int  dev_ref(struct dev *);
void dev_unref(struct dev *);
void dev_del(struct dev *);
void dev_wakeup(struct dev *);
void dev_drain(struct dev *);
struct dev *dev_new(char *, unsigned, unsigned, unsigned, unsigned, unsigned);
void dev_adjpar(struct dev *, unsigned, struct aparams *, struct aparams *);
int  devctl_add(struct dev *, char *, unsigned);
void dev_midiattach(struct dev *, struct abuf *, struct abuf *);
unsigned dev_roundof(struct dev *, unsigned);
int dev_getpos(struct dev *);
void dev_attach(struct dev *, char *, unsigned,
    struct abuf *, struct aparams *, unsigned,
    struct abuf *, struct aparams *, unsigned,
    unsigned, int);
void dev_setvol(struct dev *, struct abuf *, int);

#endif /* !define(DEV_H) */
