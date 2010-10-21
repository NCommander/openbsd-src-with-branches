/*	$OpenBSD: sock.c,v 1.51 2010/10/21 18:57:42 ratchov Exp $	*/
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

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abuf.h"
#include "aproc.h"
#include "conf.h"
#include "dev.h"
#include "midi.h"
#include "opt.h"
#include "sock.h"
#ifdef DEBUG
#include "dbg.h"
#endif

void sock_attach(struct sock *, int);
int sock_read(struct sock *);
int sock_write(struct sock *);
int sock_execmsg(struct sock *);
void sock_reset(struct sock *);
void sock_close(struct file *);

struct fileops sock_ops = {
	"sock",
	sizeof(struct sock),
       	sock_close,
	pipe_read,
	pipe_write,
	NULL, /* start */
	NULL, /* stop */
	pipe_nfds,
	pipe_pollfd,
	pipe_revents
};

#ifdef DEBUG
void
sock_dbg(struct sock *f)
{
	static char *pstates[] = {
		"hel", "ini", "sta", "rdy", "run", "stp", "mid"
	};
	static char *rstates[] = { "rdat", "rmsg", "rret" };
	static char *wstates[] = { "widl", "wmsg", "wdat" };
	struct aproc *midi;

	midi = f->dev ? f->dev->midi : NULL;
	if (f->slot >= 0 && APROC_OK(midi)) {
		dbg_puts(midi->u.ctl.slot[f->slot].name);
		dbg_putu(midi->u.ctl.slot[f->slot].unit);
	} else
		dbg_puts(f->pipe.file.name);
	dbg_puts("/");
	dbg_puts(pstates[f->pstate]);
	dbg_puts("|");
	dbg_puts(rstates[f->rstate]);
	dbg_puts("|");
	dbg_puts(wstates[f->wstate]);
}
#endif

void sock_setvol(void *, unsigned);
void sock_startreq(void *);
void sock_stopreq(void *);
void sock_quitreq(void *);
void sock_locreq(void *, unsigned);

struct ctl_ops ctl_sockops = {
	sock_setvol,
	sock_startreq,
	sock_stopreq,
	sock_locreq,
	sock_quitreq
};

unsigned sock_sesrefs = 0;	/* connections to the session */
uid_t sock_sesuid;		/* owner of the session */

void
sock_close(struct file *arg)
{
	struct sock *f = (struct sock *)arg;

	sock_sesrefs--;
	pipe_close(&f->pipe.file);
	if (f->dev) {
		dev_unref(f->dev);
		f->dev = NULL;
	}
}

void
rsock_done(struct aproc *p)
{
	struct sock *f = (struct sock *)p->u.io.file;

	if (f == NULL)
		return;
	sock_reset(f);
	f->pipe.file.rproc = NULL;
	if (f->pipe.file.wproc) {
		if (f->slot >= 0)
			ctl_slotdel(f->dev->midi, f->slot);
		aproc_del(f->pipe.file.wproc);
		file_del(&f->pipe.file);
	}
	p->u.io.file = NULL;
}

int
rsock_in(struct aproc *p, struct abuf *ibuf_dummy)
{
	struct sock *f = (struct sock *)p->u.io.file;
	struct abuf *obuf;

	if (!sock_read(f))
		return 0;
	obuf = LIST_FIRST(&p->outs);
	if (obuf && f->pstate >= SOCK_RUN) {
		if (!abuf_flush(obuf))
			return 0;
	}
	return 1;
}

int
rsock_out(struct aproc *p, struct abuf *obuf)
{
	struct sock *f = (struct sock *)p->u.io.file;

	if (f->pipe.file.state & FILE_RINUSE)
		return 0;

	/*
	 * When calling sock_read(), we may receive a ``STOP'' command,
	 * and detach ``obuf''. In this case, there's no more caller and
	 * we'll stop processing further messages, resulting in a deadlock.
	 * The solution is to iterate over sock_read() in order to
	 * consume all messages().
	 */
	for (;;) {
		if (!sock_read(f))
			return 0;
	}
	return 1;
}

void
rsock_eof(struct aproc *p, struct abuf *ibuf_dummy)
{
	aproc_del(p);
}

void
rsock_hup(struct aproc *p, struct abuf *ibuf)
{
	aproc_del(p);
}

void
rsock_opos(struct aproc *p, struct abuf *obuf, int delta)
{
	struct sock *f = (struct sock *)p->u.io.file;

	if (f->mode & MODE_RECMASK)
		return;

	f->delta += delta;
#ifdef DEBUG
	if (debug_level >= 4) {
		aproc_dbg(p);
		dbg_puts(": moved to delta = ");
		dbg_puti(f->delta);
		dbg_puts("\n");
	}
#endif
	f->tickpending++;
	for (;;) {
		if (!sock_write(f))
			break;
	}
}

struct aproc_ops rsock_ops = {
	"rsock",
	rsock_in,
	rsock_out,
	rsock_eof,
	rsock_hup,
	NULL, /* newin */
	NULL, /* newout */
	NULL, /* ipos */
	rsock_opos,
	rsock_done
};

void
wsock_done(struct aproc *p)
{
	struct sock *f = (struct sock *)p->u.io.file;

	if (f == NULL)
		return;
	sock_reset(f);
	f->pipe.file.wproc = NULL;
	if (f->pipe.file.rproc) {
		if (f->slot >= 0)
			ctl_slotdel(f->dev->midi, f->slot);
		aproc_del(f->pipe.file.rproc);
		file_del(&f->pipe.file);
	}
	p->u.io.file = NULL;
}

int
wsock_in(struct aproc *p, struct abuf *ibuf)
{
	struct sock *f = (struct sock *)p->u.io.file;

	if (f->pipe.file.state & FILE_WINUSE)
		return 0;
	/*
	 * See remark in rsock_out().
	 */
	for (;;) {
		if (!sock_write(f))
			return 0;
	}
	return 1;
}

int
wsock_out(struct aproc *p, struct abuf *obuf_dummy)
{
	struct abuf *ibuf = LIST_FIRST(&p->ins);
	struct sock *f = (struct sock *)p->u.io.file;

	if (ibuf) {
		if (!abuf_fill(ibuf))
			return 0;
	}
	if (!sock_write(f))
		return 0;
	return 1;
}

void
wsock_eof(struct aproc *p, struct abuf *obuf)
{
	aproc_del(p);
}

void
wsock_hup(struct aproc *p, struct abuf *obuf_dummy)
{
	aproc_del(p);
}

void
wsock_ipos(struct aproc *p, struct abuf *obuf, int delta)
{
	struct sock *f = (struct sock *)p->u.io.file;

	if (!(f->mode & MODE_RECMASK))
		return;

	f->delta += delta;
#ifdef DEBUG
	if (debug_level >= 4) {
		aproc_dbg(p);
		dbg_puts(": moved to delta = ");
		dbg_puti(f->delta);
		dbg_puts("\n");
	}
#endif
	f->tickpending++;
	for (;;) {
		if (!sock_write(f))
			break;
	}
}

struct aproc_ops wsock_ops = {
	"wsock",
	wsock_in,
	wsock_out,
	wsock_eof,
	wsock_hup,
	NULL, /* newin */
	NULL, /* newout */
	wsock_ipos,
	NULL, /* opos */
	wsock_done
};

/*
 * Initialise socket in the SOCK_HELLO state with default
 * parameters.
 */
struct sock *
sock_new(struct fileops *ops, int fd)
{
	struct aproc *rproc, *wproc;
	struct sock *f;
	uid_t uid, gid;

	/*
	 * ensure that all connections belong to the same user,
	 * for privacy reasons.
	 *
	 * XXX: is there a portable way of doing this ?
	 */
	if (getpeereid(fd, &uid, &gid) < 0) {
		close(fd);
		return NULL;
	}
	if (sock_sesrefs == 0) {
		/* start a new session */
		sock_sesuid = uid;
	} else if (uid != sock_sesuid) {
		/* session owned by another user, drop connection */
		close(fd);
		return NULL;
	}
	sock_sesrefs++;

	f = (struct sock *)pipe_new(ops, fd, "sock");
	if (f == NULL) {
		close(fd);
		return NULL;
	}
	f->pstate = SOCK_HELLO;
	f->mode = 0;
	f->opt = NULL;
	f->dev = NULL;
	f->xrun = XRUN_IGNORE;
	f->delta = 0;
	f->tickpending = 0;
	f->startpos = 0;
	f->startpending = 0;
	f->vol = f->lastvol = MIDI_MAXCTL;
	f->slot = -1;

	wproc = aproc_new(&wsock_ops, f->pipe.file.name);
	wproc->u.io.file = &f->pipe.file;
	wproc->u.io.partial = 0;
	f->pipe.file.wproc = wproc;
	f->wstate = SOCK_WIDLE;
	f->wtodo = 0xdeadbeef;

	rproc = aproc_new(&rsock_ops, f->pipe.file.name);
	rproc->u.io.file = &f->pipe.file;
	rproc->u.io.partial = 0;
	f->pipe.file.rproc = rproc;
	f->rstate = SOCK_RMSG;
	f->rtodo = sizeof(struct amsg);
	return f;
}

/*
 * Free buffers.
 */
void
sock_freebuf(struct sock *f)
{
	struct abuf *rbuf, *wbuf;

	f->pstate = SOCK_INIT;
#ifdef DEBUG
	if (debug_level >= 3) {
		sock_dbg(f);
		dbg_puts(": freeing buffers\n");
	}
#endif
	wbuf = LIST_FIRST(&f->pipe.file.wproc->ins);
	rbuf = LIST_FIRST(&f->pipe.file.rproc->outs);
	if (rbuf || wbuf)
		ctl_slotstop(f->dev->midi, f->slot);
	if (rbuf)
		abuf_eof(rbuf);
	if (wbuf)
		abuf_hup(wbuf);
	f->tickpending = 0;
	f->startpending = 0;
}

/*
 * Allocate buffers, so client can start filling write-end.
 */
void
sock_allocbuf(struct sock *f)
{
	struct abuf *rbuf = NULL, *wbuf = NULL;
	unsigned bufsz;

	bufsz = f->bufsz + f->dev->bufsz / f->dev->round * f->round;
	f->pstate = SOCK_START;
	if (f->mode & MODE_PLAY) {
		rbuf = abuf_new(bufsz, &f->rpar);
		aproc_setout(f->pipe.file.rproc, rbuf);
		if (!ABUF_WOK(rbuf) || (f->pipe.file.state & FILE_EOF))
			f->pstate = SOCK_READY;
		f->rmax = bufsz * aparams_bpf(&f->rpar);
	}
	if (f->mode & MODE_RECMASK) {
		wbuf = abuf_new(bufsz, &f->wpar);
		aproc_setin(f->pipe.file.wproc, wbuf);
		f->walign = f->round;
		f->wmax = 0;
	}
	f->delta = 0;
	f->startpos = 0;
	f->tickpending = 0;
	f->startpending = 0;
#ifdef DEBUG
	if (debug_level >= 3) {
		sock_dbg(f);
		dbg_puts(": allocating ");
		dbg_putu(f->bufsz);
		dbg_puts("/");
		dbg_putu(bufsz);
		dbg_puts(" fr buffers, rmax = ");
		dbg_putu(f->rmax);
		dbg_puts("\n");
	}
#endif
	if (f->mode & MODE_PLAY) {
		f->pstate = SOCK_START;
	} else {
		f->pstate = SOCK_READY;
		if (ctl_slotstart(f->dev->midi, f->slot))
			(void)sock_attach(f, 0);
	}
}

/*
 * Set volume. Callback invoked when volume is modified externally
 */
void
sock_setvol(void *arg, unsigned vol)
{
	struct sock *f = (struct sock *)arg;
	struct abuf *rbuf;

	f->vol = vol;
	rbuf = LIST_FIRST(&f->pipe.file.rproc->outs);
	if (!rbuf) {
#ifdef DEBUG
		if (debug_level >= 3) {
			sock_dbg(f);
			dbg_puts(": no read buffer to set volume yet\n");
		}
#endif
		return;
	}
	dev_setvol(f->dev, rbuf, MIDI_TO_ADATA(vol));
}

/*
 * Attach the stream. Callback invoked when MMC start
 */
void
sock_startreq(void *arg)
{
	struct sock *f = (struct sock *)arg;

#ifdef DEBUG
	if (f->pstate != SOCK_READY) {
		sock_dbg(f);
		dbg_puts(": not in READY state\n");
		dbg_panic();
	}
#endif
	(void)sock_attach(f, 0);
}

/*
 * Callback invoked by MMC stop
 */
void
sock_stopreq(void *arg)
{
#ifdef DEBUG
	struct sock *f = (struct sock *)arg;

	if (debug_level >= 3) {
		sock_dbg(f);
		dbg_puts(": ignored STOP signal\n");
	}
#endif
}

/*
 * Callback invoked by MMC relocate, ignored
 */
void
sock_locreq(void *arg, unsigned mmcpos)
{
#ifdef DEBUG
	struct sock *f = (struct sock *)arg;

	if (debug_level >= 3) {
		sock_dbg(f);
		dbg_puts(": ignored RELOCATE signal\n");
	}
#endif
}

/*
 * Callback invoked when slot is gone
 */
void
sock_quitreq(void *arg)
{
	struct sock *f = (struct sock *)arg;

#ifdef DEBUG
	if (debug_level >= 3) {
		sock_dbg(f);
		dbg_puts(": slot gone\n");
	}
#endif
	file_close(&f->pipe.file);
}

/*
 * Attach play and/or record buffers to the device
 */
void
sock_attach(struct sock *f, int force)
{
	struct abuf *rbuf, *wbuf;

	rbuf = LIST_FIRST(&f->pipe.file.rproc->outs);
	wbuf = LIST_FIRST(&f->pipe.file.wproc->ins);

	/*
	 * If in SOCK_START state, dont attach until
	 * the buffer isn't completely filled.
	 */
	if (!force && rbuf && ABUF_WOK(rbuf))
		return;

	/*
	 * start the device (dev_getpos() and dev_attach() must
	 * be called on a started device
	 */
	dev_wakeup(f->dev);

	/*
	 * get the current position, the origin is when
	 * the first sample is played/recorded
	 */
	f->startpos = dev_getpos(f->dev) * (int)f->round / (int)f->dev->round;
	f->startpending = 1;
	f->pstate = SOCK_RUN;
#ifdef DEBUG
	if (debug_level >= 3) {
		sock_dbg(f);
		dbg_puts(": attaching at ");
		dbg_puti(f->startpos);
		dbg_puts("\n");
	}
#endif
	/*
	 * We dont check whether the device is dying,
	 * because dev_xxx() functions are supposed to
	 * work (i.e., not to crash)
	 */
	dev_attach(f->dev, f->pipe.file.name, f->mode,
	    rbuf, &f->rpar,
	    f->opt->join ? f->opt->rpar.cmax - f->opt->rpar.cmin + 1 : 0,
	    wbuf, &f->wpar, 
	    f->opt->join ? f->opt->wpar.cmax - f->opt->wpar.cmin + 1 : 0,
	    f->xrun, f->opt->maxweight);
	if (f->mode & MODE_PLAY)
		dev_setvol(f->dev, rbuf, MIDI_TO_ADATA(f->vol));

	/*
	 * Send the initial position, if needed.
	 */
	for (;;) {
		if (!sock_write(f))
			break;
	}
}

void
sock_reset(struct sock *f)
{
	switch (f->pstate) {
	case SOCK_START:
	case SOCK_READY:
		if (ctl_slotstart(f->dev->midi, f->slot)) {
			(void)sock_attach(f, 1);
			f->pstate = SOCK_RUN;
		}
		/* PASSTHROUGH */
	case SOCK_RUN:
		sock_freebuf(f);
		f->pstate = SOCK_INIT;
		/* PASSTHROUGH */
	case SOCK_INIT:
		/* nothing yet */
		break;
	}
}

/*
 * Read a message from the file descriptor, return 1 if done, 0
 * otherwise. The message is stored in f->rmsg.
 */
int
sock_rmsg(struct sock *f)
{
	unsigned count;
	unsigned char *data;

	while (f->rtodo > 0) {
		if (!(f->pipe.file.state & FILE_ROK)) {
#ifdef DEBUG
			if (debug_level >= 4) {
				sock_dbg(f);
				dbg_puts(": reading message blocked, ");
				dbg_putu(f->rtodo);
				dbg_puts(" bytes remaining\n");
			}
#endif
			return 0;
		}
		data = (unsigned char *)&f->rmsg;
		data += sizeof(struct amsg) - f->rtodo;
		count = file_read(&f->pipe.file, data, f->rtodo);
		if (count == 0)
			return 0;
		f->rtodo -= count;
	}
#ifdef DEBUG
	if (debug_level >= 4) {
		sock_dbg(f);
		dbg_puts(": read full message\n");
	}
#endif
	return 1;
}

/*
 * Write a message to the file descriptor, return 1 if done, 0
 * otherwise.  The "m" argument is f->rmsg or f->wmsg, and the "ptodo"
 * points to the f->rtodo or f->wtodo respectively.
 */
int
sock_wmsg(struct sock *f, struct amsg *m, unsigned *ptodo)
{
	unsigned count;
	unsigned char *data;

	while (*ptodo > 0) {
		if (!(f->pipe.file.state & FILE_WOK)) {
#ifdef DEBUG
			if (debug_level >= 4) {
				sock_dbg(f);
				dbg_puts(": writing message blocked, ");
				dbg_putu(*ptodo);
				dbg_puts(" bytes remaining\n");
			}
#endif
			return 0;
		}
		data = (unsigned char *)m;
		data += sizeof(struct amsg) - *ptodo;
		count = file_write(&f->pipe.file, data, *ptodo);
		if (count == 0)
			return 0;
		*ptodo -= count;
	}
#ifdef DEBUG
	if (debug_level >= 4) {
		sock_dbg(f);
		dbg_puts(": wrote full message\n");
	}
#endif
	return 1;
}

/*
 * Read data chunk from the file descriptor, return 1 if at least one
 * byte was read, 0 if the file blocked.
 */
int
sock_rdata(struct sock *f)
{
	struct aproc *p;
	struct abuf *obuf;
	unsigned n;

#ifdef DEBUG
	if (f->pstate != SOCK_MIDI && f->rtodo == 0) {
		sock_dbg(f);
		dbg_puts(": data block already read\n");
		dbg_panic();
	}
#endif
	p = f->pipe.file.rproc;
	obuf = LIST_FIRST(&p->outs);
	if (obuf == NULL)
		return 0;
	if (!ABUF_WOK(obuf) || !(f->pipe.file.state & FILE_ROK))
		return 0;
	if (f->pstate == SOCK_MIDI) {
		if (!rfile_do(p, obuf->len, NULL))
			return 0;
	} else {
		if (!rfile_do(p, f->rtodo, &n))
			return 0;
		f->rtodo -= n;
		if (f->pstate == SOCK_START) {
			if (!ABUF_WOK(obuf) || (f->pipe.file.state & FILE_EOF))
				f->pstate = SOCK_READY;
		}
	}
	return 1;
}

/*
 * Write data chunk to the file descriptor, return 1 if at least one
 * byte was written, 0 if the file blocked.
 */
int
sock_wdata(struct sock *f)
{
	struct aproc *p;
	struct abuf *ibuf;
	unsigned n;

#ifdef DEBUG
	if (f->pstate != SOCK_MIDI && f->wtodo == 0) {
		sock_dbg(f);
		dbg_puts(": attempted to write zero-sized data block\n");
		dbg_panic();
	}
#endif
	if (!(f->pipe.file.state & FILE_WOK))
		return 0;
	p = f->pipe.file.wproc;
	ibuf = LIST_FIRST(&p->ins);
#ifdef DEBUG
	if (f->pstate != SOCK_MIDI && ibuf == NULL) {
		sock_dbg(f);
		dbg_puts(": attempted to write on detached buffer\n");
		dbg_panic();
	}
#endif
	if (ibuf == NULL)
		return 0;
	if (!ABUF_ROK(ibuf))
		return 0;
	if (f->pstate == SOCK_MIDI) {
		if (!wfile_do(p, ibuf->len, NULL))
			return 0;
	} else {
		if (!wfile_do(p, f->wtodo, &n))
			return 0;
		f->wtodo -= n;
	}
	return 1;
}

int
sock_setpar(struct sock *f)
{
	struct amsg_par *p = &f->rmsg.u.par;
	unsigned min, max, rate;

	if (AMSG_ISSET(p->bits)) {
		if (p->bits < BITS_MIN || p->bits > BITS_MAX) {
#ifdef DEBUG
			if (debug_level >= 1) {
				sock_dbg(f);
				dbg_puts(": ");
				dbg_putu(p->bits);
				dbg_puts(": bits out of bounds\n");
			}
#endif
			return 0;
		}
		if (AMSG_ISSET(p->bps)) {
			if (p->bps < ((p->bits + 7) / 8) || p->bps > 4) {
#ifdef DEBUG
				if (debug_level >= 1) {
					sock_dbg(f);
					dbg_puts(": ");
					dbg_putu(p->bps);
					dbg_puts(": wrong bytes per sample\n");
				}
#endif
				return 0;
			}
		} else
			p->bps = APARAMS_BPS(p->bits);
		f->rpar.bits = f->wpar.bits = p->bits;
		f->rpar.bps = f->wpar.bps = p->bps;
#ifdef DEBUG
		if (debug_level >= 3) {
			sock_dbg(f);
			dbg_puts(": using ");
			dbg_putu(p->bits);
			dbg_puts("bits, ");
			dbg_putu(p->bps);
			dbg_puts(" bytes per sample\n");
		}
#endif
	}
	if (AMSG_ISSET(p->sig))
		f->rpar.sig = f->wpar.sig = p->sig ? 1 : 0;
	if (AMSG_ISSET(p->le))
		f->rpar.le = f->wpar.le = p->le ? 1 : 0;
	if (AMSG_ISSET(p->msb))
		f->rpar.msb = f->wpar.msb = p->msb ? 1 : 0;
	if (AMSG_ISSET(p->rchan) && (f->mode & MODE_RECMASK)) {
		if (p->rchan < 1)
			p->rchan = 1;
		if (p->rchan > NCHAN_MAX)
			p->rchan = NCHAN_MAX;
		f->wpar.cmin = f->opt->wpar.cmin;
		f->wpar.cmax = f->opt->wpar.cmin + p->rchan - 1;
		if (f->wpar.cmax > f->opt->wpar.cmax)
			f->wpar.cmax = f->opt->wpar.cmax;
#ifdef DEBUG
		if (debug_level >= 3) {
			sock_dbg(f);
			dbg_puts(": using recording channels ");
			dbg_putu(f->wpar.cmin);
			dbg_puts("..");
			dbg_putu(f->wpar.cmax);
			dbg_puts("\n");
		}
#endif
	}
	if (AMSG_ISSET(p->pchan) && (f->mode & MODE_PLAY)) {
		if (p->pchan < 1)
			p->pchan = 1;
		if (p->pchan > NCHAN_MAX)
			p->pchan = NCHAN_MAX;
		f->rpar.cmin = f->opt->rpar.cmin;
		f->rpar.cmax = f->opt->rpar.cmin + p->pchan - 1;
		if (f->rpar.cmax > f->opt->rpar.cmax)
			f->rpar.cmax = f->opt->rpar.cmax;
#ifdef DEBUG
		if (debug_level >= 3) {
			sock_dbg(f);
			dbg_puts(": using playback channels ");
			dbg_putu(f->rpar.cmin);
			dbg_puts("..");
			dbg_putu(f->rpar.cmax);
			dbg_puts("\n");
		}
#endif
	}
	if (AMSG_ISSET(p->rate)) {
		if (p->rate < RATE_MIN)
			p->rate = RATE_MIN;
		if (p->rate > RATE_MAX)
			p->rate = RATE_MAX;
		f->round = dev_roundof(f->dev, p->rate);
		f->rpar.rate = f->wpar.rate = p->rate;
		if (!AMSG_ISSET(p->appbufsz)) {
			p->appbufsz = f->dev->bufsz / f->dev->round * f->round;
#ifdef DEBUG
			if (debug_level >= 3) {
				sock_dbg(f);
				dbg_puts(": using ");
				dbg_putu(p->appbufsz);
				dbg_puts(" fr app buffer size\n");
			}
#endif
		}
#ifdef DEBUG
		if (debug_level >= 3) {
			sock_dbg(f);
			dbg_puts(": using ");
			dbg_putu(p->rate);
			dbg_puts("Hz sample rate, ");
			dbg_putu(f->round);
			dbg_puts(" fr block size\n");
		}
#endif
	}
	if (AMSG_ISSET(p->xrun)) {
		if (p->xrun != XRUN_IGNORE &&
		    p->xrun != XRUN_SYNC &&
		    p->xrun != XRUN_ERROR) {
#ifdef DEBUG
			if (debug_level >= 1) {
				sock_dbg(f);
				dbg_puts(": ");
				dbg_putx(p->xrun);
				dbg_puts(": bad xrun policy\n");
			}
#endif
			return 0;
		}
		f->xrun = p->xrun;
		if (f->opt->mmc && f->xrun == XRUN_IGNORE)
			f->xrun = XRUN_SYNC;
#ifdef DEBUG
		if (debug_level >= 3) {
			sock_dbg(f);
			dbg_puts(": using 0x");
			dbg_putx(f->xrun);
			dbg_puts(" xrun policy\n");
		}
#endif
	}
	if (AMSG_ISSET(p->appbufsz)) {
		rate = (f->mode & MODE_PLAY) ? f->rpar.rate : f->wpar.rate;
		min = 1;
		max = 1 + rate / f->dev->round;
		min *= f->round;
		max *= f->round;
		p->appbufsz += f->round - 1;
		p->appbufsz -= p->appbufsz % f->round;
		if (p->appbufsz < min)
			p->appbufsz = min;
		if (p->appbufsz > max)
			p->appbufsz = max;
		f->bufsz = p->appbufsz;
#ifdef DEBUG
		if (debug_level >= 3) {
			sock_dbg(f);
			dbg_puts(": using ");
			dbg_putu(f->bufsz);
			dbg_puts(" buffer size\n");
		}
#endif
	}
#ifdef DEBUG
	if (debug_level >= 2) {
		if (APROC_OK(f->dev->midi)) {
			dbg_puts(f->dev->midi->u.ctl.slot[f->slot].name);
			dbg_putu(f->dev->midi->u.ctl.slot[f->slot].unit);
		} else
			dbg_puts(f->pipe.file.name);
		dbg_puts(": buffer size = ");
		dbg_putu(f->bufsz);
		if (f->mode & MODE_PLAY) {
			dbg_puts(", play = ");
			aparams_dbg(&f->rpar);
		}
		if (f->mode & MODE_RECMASK) {
			dbg_puts(", rec:");
			aparams_dbg(&f->wpar);
		}
		dbg_puts("\n");
	}
#endif
	return 1;
}

/*
 * allocate buffers, so client can start filling write-end.
 */
void
sock_midiattach(struct sock *f)
{
	struct abuf *rbuf = NULL, *wbuf = NULL;
	
	if (f->mode & MODE_MIDIOUT) {
		rbuf = abuf_new(MIDI_BUFSZ, &aparams_none);
		aproc_setout(f->pipe.file.rproc, rbuf);
	}
	if (f->mode & MODE_MIDIIN) {
		wbuf = abuf_new(MIDI_BUFSZ, &aparams_none);
		aproc_setin(f->pipe.file.wproc, wbuf);
	}
	f->pstate = SOCK_MIDI;
	dev_midiattach(f->dev, rbuf, wbuf);
}

int
sock_hello(struct sock *f)
{
	struct amsg_hello *p = &f->rmsg.u.hello;

#ifdef DEBUG
	if (debug_level >= 3) {
		sock_dbg(f);
		dbg_puts(": hello from <");
		dbg_puts(p->who);
		dbg_puts(">, mode = ");
		dbg_putx(p->mode);
		dbg_puts(", ver ");
		dbg_putu(p->version);
		dbg_puts("\n");
	}
#endif
	if (p->version != AMSG_VERSION) {
#ifdef DEBUG
		if (debug_level >= 1) {
			sock_dbg(f);
			dbg_puts(": ");
			dbg_putu(p->version);
			dbg_puts(": unsupported protocol version\n");
		}
#endif
		return 0;
	}
	switch (p->mode) {
	case MODE_MIDIIN:
	case MODE_MIDIOUT:
	case MODE_MIDIOUT | MODE_MIDIIN:
	case MODE_REC:
	case MODE_PLAY:
	case MODE_PLAY | MODE_REC:
		break;
	default:
#ifdef DEBUG
		if (debug_level >= 1) {
			sock_dbg(f);
			dbg_puts(": ");
			dbg_putx(p->mode);
			dbg_puts(": unsupported mode\n");
		}
#endif
		return 0;
	}
	f->opt = opt_byname(p->opt);
	if (f->opt == NULL)
		return 0;
	if (!dev_ref(f->opt->dev))
		return 0;
	if ((p->mode & MODE_REC) && (f->opt->mode & MODE_MON)) {
		p->mode &= ~MODE_REC;
		p->mode |= MODE_MON;
	}
	f->dev = f->opt->dev;
	f->mode = (p->mode & f->opt->mode) & f->dev->mode;
#ifdef DEBUG
	if (debug_level >= 3) {
		sock_dbg(f);
		dbg_puts(": using mode = ");
		dbg_putx(f->mode);
		dbg_puts("\n");
	}
#endif
	if (f->mode != p->mode) {
#ifdef DEBUG
		if (debug_level >= 1) {
			sock_dbg(f);
			dbg_puts(": requested mode not available\n");
		}
#endif
		return 0;
	}
	if (f->mode & (MODE_MIDIOUT | MODE_MIDIIN)) {
		sock_midiattach(f);
		return 1;
	}
	if (f->mode & MODE_PLAY)
		f->rpar = f->opt->rpar;
	if (f->mode & MODE_RECMASK)
		f->wpar = f->opt->wpar;
	f->xrun = (f->opt->mmc) ? XRUN_SYNC : XRUN_IGNORE;
	f->bufsz = f->dev->bufsz;
	f->round = f->dev->round;
	f->slot = ctl_slotnew(f->dev->midi, p->who,
	    &ctl_sockops, f,
	    f->opt->mmc);
	if (f->slot < 0)
		return 0;
	f->pstate = SOCK_INIT;
	return 1;
}

/*
 * Execute message in f->rmsg and change the state accordingly; return 1
 * on success, and 0 on failure, in which case the socket is destroyed.
 */
int
sock_execmsg(struct sock *f)
{
	struct amsg *m = &f->rmsg;
	struct abuf *obuf;

	switch (m->cmd) {
	case AMSG_DATA:
#ifdef DEBUG
		if (debug_level >= 4) {
			sock_dbg(f);
			dbg_puts(": DATA message\n");
		}
#endif
		if (f->pstate != SOCK_RUN && f->pstate != SOCK_START &&
		    f->pstate != SOCK_READY) {
#ifdef DEBUG
			if (debug_level >= 1) {
				sock_dbg(f);
				dbg_puts(": DATA, bad state\n");
			}
#endif
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		if (!(f->mode & MODE_PLAY)) {
#ifdef DEBUG
			if (debug_level >= 1) {
				sock_dbg(f);
				dbg_puts(": DATA not allowed in record-only mode\n");
			}
#endif
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		obuf = LIST_FIRST(&f->pipe.file.rproc->outs);
		if (f->pstate == SOCK_START && !ABUF_WOK(obuf)) {
#ifdef DEBUG
			if (debug_level >= 1) {
				sock_dbg(f);
				dbg_puts(": DATA client violates flow control\n");
			}
#endif
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		if (m->u.data.size % obuf->bpf != 0) {
#ifdef DEBUG
			if (debug_level >= 1) {
				sock_dbg(f);
				dbg_puts(": unaligned data chunk\n");
			}
#endif
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		f->rstate = SOCK_RDATA;
		f->rtodo = m->u.data.size / obuf->bpf;
#ifdef DEBUG
		if (f->rtodo > f->rmax && debug_level >= 2) {
			sock_dbg(f);
			dbg_puts(": received past current position, rtodo = ");
			dbg_putu(f->rtodo);
			dbg_puts(", rmax = ");
			dbg_putu(f->rmax);
			dbg_puts("\n");
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
#endif
		f->rmax -= f->rtodo;
		if (f->rtodo == 0) {
#ifdef DEBUG
			if (debug_level >= 1) {
				sock_dbg(f);
				dbg_puts(": zero-length data chunk\n");
			}
#endif
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		break;
	case AMSG_START:
#ifdef DEBUG
		if (debug_level >= 3) {
			sock_dbg(f);
			dbg_puts(": START message\n");
		}
#endif
		if (f->pstate != SOCK_INIT) {
#ifdef DEBUG
			if (debug_level >= 1) {
				sock_dbg(f);
				dbg_puts(": START, bad state\n");
			}
#endif
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		sock_allocbuf(f);
		f->rstate = SOCK_RMSG;
		f->rtodo = sizeof(struct amsg);
		break;
	case AMSG_STOP:
#ifdef DEBUG
		if (debug_level >= 3) {
			sock_dbg(f);
			dbg_puts(": STOP message\n");
		}
#endif
		if (f->pstate != SOCK_RUN &&
		    f->pstate != SOCK_START && f->pstate != SOCK_READY) {
#ifdef DEBUG
			if (debug_level >= 1) {
				sock_dbg(f);
				dbg_puts(": STOP, bad state\n");
			}
#endif
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		/*
		 * XXX: device could have desappeared at this point,
		 * see how this is fixed in wav.c
		 */
		if ((f->pstate == SOCK_START || f->pstate == SOCK_READY) &&
		    ctl_slotstart(f->dev->midi, f->slot))
			(void)sock_attach(f, 1);
		if (f->wstate != SOCK_WDATA || f->wtodo == 0)
			sock_freebuf(f);
		else
			f->pstate = SOCK_STOP;
		AMSG_INIT(m);
		m->cmd = AMSG_ACK;
		f->rstate = SOCK_RRET;
		f->rtodo = sizeof(struct amsg);
		break;
	case AMSG_SETPAR:
#ifdef DEBUG
		if (debug_level >= 3) {
			sock_dbg(f);
			dbg_puts(": SETPAR message\n");
		}
#endif
		if (f->pstate != SOCK_INIT) {
#ifdef DEBUG
			if (debug_level >= 1) {
				sock_dbg(f);
				dbg_puts(": SETPAR, bad state\n");
			}
#endif
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		if (!sock_setpar(f)) {
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		f->rtodo = sizeof(struct amsg);
		f->rstate = SOCK_RMSG;
		break;
	case AMSG_GETPAR:
#ifdef DEBUG
		if (debug_level >= 3) {
			sock_dbg(f);
			dbg_puts(": GETPAR message\n");
		}
#endif
		if (f->pstate != SOCK_INIT) {
#ifdef DEBUG
			if (debug_level >= 1) {
				sock_dbg(f);
				dbg_puts(": GETPAR, bad state\n");
			}
#endif
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		AMSG_INIT(m);
		m->cmd = AMSG_GETPAR;
		m->u.par.legacy_mode = f->mode;
		if (f->mode & MODE_PLAY) {
			m->u.par.bits = f->rpar.bits;
			m->u.par.bps = f->rpar.bps;
			m->u.par.sig = f->rpar.sig;
			m->u.par.le = f->rpar.le;
			m->u.par.msb = f->rpar.msb;
			m->u.par.rate = f->rpar.rate;
			m->u.par.pchan = f->rpar.cmax - f->rpar.cmin + 1;
		}
		if (f->mode & MODE_RECMASK) {
			m->u.par.bits = f->wpar.bits;
			m->u.par.bps = f->wpar.bps;
			m->u.par.sig = f->wpar.sig;
			m->u.par.le = f->wpar.le;
			m->u.par.msb = f->wpar.msb;
			m->u.par.rate = f->wpar.rate;
			m->u.par.rchan = f->wpar.cmax - f->wpar.cmin + 1;
		}
		m->u.par.appbufsz = f->bufsz;
		m->u.par.bufsz =
		    f->bufsz + (f->dev->bufsz / f->dev->round) * f->round;
		m->u.par.round = f->round;
		f->rstate = SOCK_RRET;
		f->rtodo = sizeof(struct amsg);
		break;
	case AMSG_GETCAP:
#ifdef DEBUG
		if (debug_level >= 3) {
			sock_dbg(f);
			dbg_puts(": GETCAP message\n");
		}
#endif
		if (f->pstate != SOCK_INIT) {
#ifdef DEBUG
			if (debug_level >= 1) {
				sock_dbg(f);
				dbg_puts(": GETCAP, bad state\n");
			}
#endif
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		AMSG_INIT(m);
		m->cmd = AMSG_GETCAP;
		m->u.cap.rate = f->dev->rate;
		m->u.cap.pchan = (f->opt->mode & MODE_PLAY) ?
		    (f->opt->rpar.cmax - f->opt->rpar.cmin + 1) : 0;
		m->u.cap.rchan = (f->opt->mode & (MODE_PLAY | MODE_REC)) ?
		    (f->opt->wpar.cmax - f->opt->wpar.cmin + 1) : 0;
		m->u.cap.bits = sizeof(short) * 8;
		m->u.cap.bps = sizeof(short);
		f->rstate = SOCK_RRET;
		f->rtodo = sizeof(struct amsg);
		break;
	case AMSG_SETVOL:
#ifdef DEBUG
		if (debug_level >= 3) {
			sock_dbg(f);
			dbg_puts(": SETVOL message\n");
		}
#endif
		if (f->pstate != SOCK_RUN && f->pstate != SOCK_START &&
		    f->pstate != SOCK_INIT && f->pstate != SOCK_READY) {
#ifdef DEBUG
			if (debug_level >= 1) {
				sock_dbg(f);
				dbg_puts(": SETVOL, bad state\n");
			}
#endif
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		if (m->u.vol.ctl > MIDI_MAXCTL) {
#ifdef DEBUG
			if (debug_level >= 1) {
				sock_dbg(f);
				dbg_puts(": SETVOL, volume out of range\n");
			}
#endif
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		sock_setvol(f, m->u.vol.ctl);
		if (f->slot >= 0)
			ctl_slotvol(f->dev->midi, f->slot, m->u.vol.ctl);
		f->rtodo = sizeof(struct amsg);
		f->rstate = SOCK_RMSG;
		break;
	case AMSG_HELLO:
#ifdef DEBUG
		if (debug_level >= 3) {
			sock_dbg(f);
			dbg_puts(": HELLO message\n");
		}
#endif
		if (f->pstate != SOCK_HELLO) {
#ifdef DEBUG
			if (debug_level >= 1) {
				sock_dbg(f);
				dbg_puts(": HELLO, bad state\n");
			}
#endif
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		if (!sock_hello(f)) {
			aproc_del(f->pipe.file.rproc);
			return 0;
		}
		AMSG_INIT(m);
		m->cmd = AMSG_ACK;
		f->rstate = SOCK_RRET;
		f->rtodo = sizeof(struct amsg);
		break;
	case AMSG_BYE:
#ifdef DEBUG
		if (debug_level >= 3) {
			sock_dbg(f);
			dbg_puts(": BYE message\n");
		}
#endif
		if (f->pstate != SOCK_INIT) {
#ifdef DEBUG
			if (debug_level >= 1) {
				sock_dbg(f);
				dbg_puts(": BYE, bad state\n");
			}
#endif
		}
		aproc_del(f->pipe.file.rproc);
		return 0;
	default:
#ifdef DEBUG
		if (debug_level >= 1) {
			sock_dbg(f);
			dbg_puts(": unknown command in message\n");
		}
#endif
		aproc_del(f->pipe.file.rproc);
		return 0;
	}
	if (f->rstate == SOCK_RRET) {
		if (f->wstate != SOCK_WIDLE ||
		    !sock_wmsg(f, &f->rmsg, &f->rtodo))
			return 0;
#ifdef DEBUG
		if (debug_level >= 3) {
			sock_dbg(f);
			dbg_puts(": RRET done\n");
		}
#endif
		if (f->pstate == SOCK_MIDI && (f->mode & MODE_MIDIOUT)) {
			f->rstate = SOCK_RDATA;
			f->rtodo = 0;
		} else {
			f->rstate = SOCK_RMSG;
			f->rtodo = sizeof(struct amsg);
		}
	}
	return 1;
}

/*
 * Create a new data/pos message.
 */
int
sock_buildmsg(struct sock *f)
{
	struct aproc *p;
	struct abuf *ibuf;
	unsigned size, max;

	if (f->pstate == SOCK_MIDI) {
#ifdef DEBUG
		if (debug_level >= 3) {
			sock_dbg(f);
			dbg_puts(": switching to MIDI mode\n");
		}
#endif
		f->wstate = SOCK_WDATA;
		f->wtodo = 0;
		return 1;
	}

	/*
	 * Send initial position
	 */
	if (f->startpending) {
#ifdef DEBUG
		if (debug_level >= 4) {
			sock_dbg(f);
			dbg_puts(": building POS message, pos = ");
			dbg_puti(f->startpos);
			dbg_puts("\n");
		}
#endif
		AMSG_INIT(&f->wmsg);
		f->wmsg.cmd = AMSG_POS;
		f->wmsg.u.ts.delta = f->startpos;
		f->rmax += f->startpos;
		f->wtodo = sizeof(struct amsg);
		f->wstate = SOCK_WMSG;
		f->startpending = 0;
		return 1;
	}

	/*
	 * If pos changed, build a MOVE message.
	 */
	if (f->tickpending) {
#ifdef DEBUG
		if (debug_level >= 4) {
			sock_dbg(f);
			dbg_puts(": building MOVE message, delta = ");
			dbg_puti(f->delta);
			dbg_puts("\n");
		}
#endif
		f->wmax += f->delta;
		f->rmax += f->delta;
		AMSG_INIT(&f->wmsg);
		f->wmsg.cmd = AMSG_MOVE;
		f->wmsg.u.ts.delta = f->delta;
		f->wtodo = sizeof(struct amsg);
		f->wstate = SOCK_WMSG;
		f->delta = 0;
		f->tickpending = 0;
		return 1;
	}

	/*
	 * if volume changed build a SETVOL message
	 */
	if (f->pstate >= SOCK_START && f->vol != f->lastvol) {
#ifdef DEBUG
		if (debug_level >= 4) {
			sock_dbg(f);
			dbg_puts(": building SETVOL message, vol = ");
			dbg_puti(f->vol);
			dbg_puts("\n");
		}
#endif
		AMSG_INIT(&f->wmsg);
		f->wmsg.cmd = AMSG_SETVOL;
		f->wmsg.u.vol.ctl = f->vol;
		f->wtodo = sizeof(struct amsg);
		f->wstate = SOCK_WMSG;
		f->lastvol = f->vol;
		return 1;
	}

	/*
	 * If data available, build a DATA message.
	 */
	p = f->pipe.file.wproc;
	ibuf = LIST_FIRST(&p->ins);
	if (ibuf && ABUF_ROK(ibuf)) {
#ifdef DEBUG
		if (ibuf->used > f->wmax && debug_level >= 3) {
			sock_dbg(f);
			dbg_puts(": attempt to send past current position: used = ");
			dbg_putu(ibuf->used);
			dbg_puts(" wmax = ");
			dbg_putu(f->wmax);
			dbg_puts("\n");
		}
#endif
		max = AMSG_DATAMAX / ibuf->bpf;
		size = ibuf->used;
		if (size > f->walign)
			size = f->walign;
		if (size > f->wmax)
			size = f->wmax;
		if (size > max)
			size = max;
		if (size == 0)
			return 0;
		f->walign -= size;
		f->wmax -= size;
		if (f->walign == 0)
			f->walign = f->round;
		AMSG_INIT(&f->wmsg);
		f->wmsg.cmd = AMSG_DATA;
		f->wmsg.u.data.size = size * ibuf->bpf;
		f->wtodo = sizeof(struct amsg);
		f->wstate = SOCK_WMSG;
		return 1;
	}
#ifdef DEBUG
	if (debug_level >= 4) {
		sock_dbg(f);
		dbg_puts(": no messages to build anymore, idling...\n");
	}
#endif
	f->wstate = SOCK_WIDLE;
	return 0;
}

/*
 * Read from the socket file descriptor, fill input buffer and update
 * the state. Return 1 if at least one message or 1 data byte was
 * processed, 0 if something blocked.
 */
int
sock_read(struct sock *f)
{
#ifdef DEBUG
	if (debug_level >= 4) {
		sock_dbg(f);
		dbg_puts(": reading ");
		dbg_putu(f->rtodo);
		dbg_puts(" todo\n");
	}
#endif
	switch (f->rstate) {
	case SOCK_RMSG:
		if (!sock_rmsg(f))
			return 0;
		if (!sock_execmsg(f))
			return 0;
		break;
	case SOCK_RDATA:
		if (!sock_rdata(f))
			return 0;
		if (f->pstate != SOCK_MIDI && f->rtodo == 0) {
			f->rstate = SOCK_RMSG;
			f->rtodo = sizeof(struct amsg);
		}
		/*
		 * XXX: sock_attach() may not start if there's not enough
		 *	samples queued, if so ctl_slotstart() will trigger
		 *	other streams, but this one won't start.
		 */
		if (f->pstate == SOCK_READY && ctl_slotstart(f->dev->midi, f->slot))
			(void)sock_attach(f, 0);
		break;
	case SOCK_RRET:
#ifdef DEBUG
		if (debug_level >= 4) {
			sock_dbg(f);
			dbg_puts(": blocked by pending RRET message\n");
		}
#endif
		return 0;
	}
	return 1;
}

/*
 * Process messages to return.
 */
int
sock_return(struct sock *f)
{
	struct aproc *rp;

	while (f->rstate == SOCK_RRET) {
		if (!sock_wmsg(f, &f->rmsg, &f->rtodo))
			return 0;
#ifdef DEBUG
		if (debug_level >= 4) {
			sock_dbg(f);
			dbg_puts(": sent RRET message\n");
		}
#endif
		if (f->pstate == SOCK_MIDI && (f->mode & MODE_MIDIOUT)) {
			f->rstate = SOCK_RDATA;
			f->rtodo = 0;
		} else {
			f->rstate = SOCK_RMSG;
			f->rtodo = sizeof(struct amsg);
		}
		if (f->pipe.file.state & FILE_RINUSE)
			break;
		f->pipe.file.state |= FILE_RINUSE;
		for (;;) {
			/*
			 * in() may trigger rsock_done and destroy the
			 * wsock.
			 */
			rp = f->pipe.file.rproc;
			if (!rp || !rp->ops->in(rp, NULL))
				break;
		}
		f->pipe.file.state &= ~FILE_RINUSE;
		if (f->pipe.file.wproc == NULL)
			return 0;
	}
	return 1;
}

/*
 * Write messages and data on the socket file descriptor. Return 1 if
 * at least one message or one data byte was processed, 0 if something
 * blocked.
 */
int
sock_write(struct sock *f)
{
#ifdef DEBUG
	if (debug_level >= 4) {
		sock_dbg(f);
		dbg_puts(": writing ");
		dbg_putu(f->wtodo);
		dbg_puts(" todo\n");
	}
#endif
	switch (f->wstate) {
	case SOCK_WMSG:
		if (!sock_wmsg(f, &f->wmsg, &f->wtodo))
			return 0;
		if (f->wmsg.cmd != AMSG_DATA) {
			f->wstate = SOCK_WIDLE;
			f->wtodo = 0xdeadbeef;
			break;
		}
		/*
		 * XXX: why not set f->wtodo in sock_wmsg() ?
		 */
		f->wstate = SOCK_WDATA;
		f->wtodo = f->wmsg.u.data.size /
		    LIST_FIRST(&f->pipe.file.wproc->ins)->bpf;
		/* PASSTHROUGH */
	case SOCK_WDATA:
		if (!sock_wdata(f))
			return 0;
		if (f->pstate == SOCK_MIDI || f->wtodo > 0)
			break;
		f->wstate = SOCK_WIDLE;
		f->wtodo = 0xdeadbeef;
		if (f->pstate == SOCK_STOP)
			sock_freebuf(f);
		/* PASSTHROUGH */
	case SOCK_WIDLE:
		if (!sock_return(f))
			return 0;
		if (!sock_buildmsg(f))
			return 0;
		break;
#ifdef DEBUG
	default:
		sock_dbg(f);
		dbg_puts(": bad writing end state\n");
		dbg_panic();
#endif
	}
	return 1;
}
