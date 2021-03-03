/*	$OpenBSD: siofile.c,v 1.23 2021/03/02 12:15:46 edd Exp $	*/
/*
 * Copyright (c) 2008-2012 Alexandre Ratchov <alex@caoua.org>
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
#include <sys/time.h>
#include <sys/types.h>

#include <poll.h>
#include <sndio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abuf.h"
#include "defs.h"
#include "dev.h"
#include "dev_sioctl.h"
#include "dsp.h"
#include "fdpass.h"
#include "file.h"
#include "siofile.h"
#include "utils.h"

#define WATCHDOG_USEC	4000000		/* 4 seconds */

void dev_sio_onmove(void *, int);
void dev_sio_timeout(void *);
int dev_sio_pollfd(void *, struct pollfd *);
int dev_sio_revents(void *, struct pollfd *);
void dev_sio_run(void *);
void dev_sio_hup(void *);

extern struct fileops dev_sioctl_ops;

struct fileops dev_sio_ops = {
	"sio",
	dev_sio_pollfd,
	dev_sio_revents,
	dev_sio_run,
	dev_sio_run,
	dev_sio_hup
};

void
dev_sio_onmove(void *arg, int delta)
{
	struct dev *d = arg;

#ifdef DEBUG
	if (log_level >= 4) {
		dev_log(d);
		log_puts(": tick, delta = ");
		log_puti(delta);
		log_puts("\n");
	}
	d->sio.sum_utime += file_utime - d->sio.utime;
	d->sio.sum_wtime += file_wtime - d->sio.wtime;
	d->sio.wtime = file_wtime;
	d->sio.utime = file_utime;
	if (d->mode & MODE_PLAY)
		d->sio.pused -= delta;
	if (d->mode & MODE_REC)
		d->sio.rused += delta;
#endif
	dev_onmove(d, delta);
}

void
dev_sio_timeout(void *arg)
{
	struct dev *d = arg;

	dev_log(d);
	log_puts(": watchdog timeout\n");
	dev_abort(d);
}

static int
dev_sio_openalt(struct dev *d, struct dev_alt *n,
    struct sio_hdl **rhdl, struct sioctl_hdl **rctlhdl, unsigned int *rmode)
{
	struct sio_hdl *hdl;
	struct sioctl_hdl *ctlhdl;
	unsigned int mode = d->reqmode & (MODE_PLAY | MODE_REC);

	hdl = fdpass_sio_open(d->num, n->idx, mode);
	if (hdl == NULL) {
		if (mode != (SIO_PLAY | SIO_REC))
			return 0;
		hdl = fdpass_sio_open(d->num, n->idx, SIO_PLAY);
		if (hdl != NULL)
			mode = SIO_PLAY;
		else {
			hdl = fdpass_sio_open(d->num, n->idx, SIO_REC);
			if (hdl != NULL)
				mode = SIO_REC;
			else
				return 0;
		}
		if (log_level >= 1) {
			log_puts("warning, device opened in ");
			log_puts(mode == SIO_PLAY ? "play-only" : "rec-only");
			log_puts(" mode\n");
		}
	}

	ctlhdl = fdpass_sioctl_open(d->num, n->idx, SIOCTL_READ | SIOCTL_WRITE);
	if (ctlhdl == NULL) {
		if (log_level >= 1) {
			dev_log(d);
			log_puts(": no control device\n");
		}
	}

	*rhdl = hdl;
	*rctlhdl = ctlhdl;
	*rmode = mode;
	return 1;
}

/*
 * open the device using one of the provided paths
 */
static int
dev_sio_openlist(struct dev *d,
    struct sio_hdl **rhdl, struct sioctl_hdl **rctlhdl, unsigned int *rmode)
{
	struct dev_alt *n;
	struct ctl *c;
	int val;

	for (n = d->alt_list; n != NULL; n = n->next) {
		if (d->alt_num == n->idx)
			continue;
		if (log_level >= 2) {
			dev_log(d);
			log_puts(": trying ");
			log_puts(n->name);
			log_puts("\n");
		}
		if (dev_sio_openalt(d, n, rhdl, rctlhdl, rmode)) {
			if (log_level >= 2) {
				dev_log(d);
				log_puts(": using ");
				log_puts(n->name);
				log_puts("\n");
			}
			d->alt_num = n->idx;
			for (c = ctl_list; c != NULL; c = c->next) {
				if (!ctl_match(c, CTL_DEV_ALT, d, NULL))
					continue;
				val = c->u.dev_alt.idx == n->idx;
				if (c->curval == val)
					continue;
				c->curval = val;
				if (val)
					c->val_mask = ~0U;
			}
			return 1;
		}
	}
	return 0;
}

/*
 * open the device.
 */
int
dev_sio_open(struct dev *d)
{
	struct sio_par par;

	if (!dev_sio_openlist(d, &d->sio.hdl, &d->sioctl.hdl, &d->mode))
		return 0;

	sio_initpar(&par);
	par.bits = d->par.bits;
	par.bps = d->par.bps;
	par.sig = d->par.sig;
	par.le = d->par.le;
	par.msb = d->par.msb;
	if (d->mode & SIO_PLAY)
		par.pchan = d->pchan;
	if (d->mode & SIO_REC)
		par.rchan = d->rchan;
	if (d->bufsz)
		par.appbufsz = d->bufsz;
	if (d->round)
		par.round = d->round;
	if (d->rate)
		par.rate = d->rate;
	if (!sio_setpar(d->sio.hdl, &par))
		goto bad_close;
	if (!sio_getpar(d->sio.hdl, &par))
		goto bad_close;

#ifdef DEBUG
	/*
	 * We support any parameter combination exposed by the kernel,
	 * and we have no other choice than trusting the kernel for
	 * returning correct parameters. But let's check parameters
	 * early and nicely report kernel bugs rather than crashing
	 * later in memset(), malloc() or alike.
	 */

	if (par.bits > BITS_MAX) {
		dev_log(d);
		log_puts(": ");
		log_putu(par.bits);
		log_puts(": unsupported number of bits\n");
		goto bad_close;
	}
	if (par.bps > SIO_BPS(BITS_MAX)) {
		dev_log(d);
		log_puts(": ");
		log_putu(par.bps);
		log_puts(": unsupported sample size\n");
		goto bad_close;
	}
	if ((d->mode & SIO_PLAY) && par.pchan > NCHAN_MAX) {
		dev_log(d);
		log_puts(": ");
		log_putu(par.pchan);
		log_puts(": unsupported number of play channels\n");
		goto bad_close;
	}
	if ((d->mode & SIO_REC) && par.rchan > NCHAN_MAX) {
		dev_log(d);
		log_puts(": ");
		log_putu(par.rchan);
		log_puts(": unsupported number of rec channels\n");
		goto bad_close;
	}
	if (par.bufsz == 0 || par.bufsz > RATE_MAX) {
		dev_log(d);
		log_puts(": ");
		log_putu(par.bufsz);
		log_puts(": unsupported buffer size\n");
		goto bad_close;
	}
	if (par.round == 0 || par.round > par.bufsz ||
	    par.bufsz % par.round != 0) {
		dev_log(d);
		log_puts(": ");
		log_putu(par.round);
		log_puts(": unsupported block size\n");
		goto bad_close;
	}
	if (par.rate == 0 || par.rate > RATE_MAX) {
		dev_log(d);
		log_puts(": ");
		log_putu(par.rate);
		log_puts(": unsupported rate\n");
		goto bad_close;
	}
#endif

	d->par.bits = par.bits;
	d->par.bps = par.bps;
	d->par.sig = par.sig;
	d->par.le = par.le;
	d->par.msb = par.msb;
	if (d->mode & SIO_PLAY)
		d->pchan = par.pchan;
	if (d->mode & SIO_REC)
		d->rchan = par.rchan;
	d->bufsz = par.bufsz;
	d->round = par.round;
	d->rate = par.rate;
	if (d->mode & MODE_PLAY)
		d->mode |= MODE_MON;
	sio_onmove(d->sio.hdl, dev_sio_onmove, d);
	d->sio.file = file_new(&dev_sio_ops, d, "dev", sio_nfds(d->sio.hdl));
	if (d->sioctl.hdl) {
		d->sioctl.file = file_new(&dev_sioctl_ops, d, "mix",
		    sioctl_nfds(d->sioctl.hdl));
	}
	timo_set(&d->sio.watchdog, dev_sio_timeout, d);
	dev_sioctl_open(d);
	return 1;
 bad_close:
	sio_close(d->sio.hdl);
	if (d->sioctl.hdl) {
		sioctl_close(d->sioctl.hdl);
		d->sioctl.hdl = NULL;
	}
	return 0;
}

/*
 * Open an alternate device. Upon success and if the new device is
 * compatible with the old one, close the old device and continue
 * using the new one. The new device is not started.
 */
int
dev_sio_reopen(struct dev *d)
{
	struct sio_par par;
	struct sio_hdl *hdl;
	struct sioctl_hdl *ctlhdl;
	unsigned int mode;

	if (!dev_sio_openlist(d, &hdl, &ctlhdl, &mode))
		return 0;

	sio_initpar(&par);
	par.bits = d->par.bits;
	par.bps = d->par.bps;
	par.sig = d->par.sig;
	par.le = d->par.le;
	par.msb = d->par.msb;
	if (mode & SIO_PLAY)
		par.pchan = d->reqpchan;
	if (mode & SIO_REC)
		par.rchan = d->reqrchan;
	par.appbufsz = d->bufsz;
	par.round = d->round;
	par.rate = d->rate;
	if (!sio_setpar(hdl, &par))
		goto bad_close;
	if (!sio_getpar(hdl, &par))
		goto bad_close;

	/* check if new parameters are compatible with old ones */
	if (par.round != d->round || par.bufsz != d->bufsz ||
	    par.rate != d->rate) {
		if (log_level >= 1) {
			dev_log(d);
			log_puts(": alternate device not compatible\n");
		}
		goto bad_close;
	}

	/* close unused device */
	timo_del(&d->sio.watchdog);
	file_del(d->sio.file);
	sio_close(d->sio.hdl);
	if (d->sioctl.hdl) {
		file_del(d->sioctl.file);
		sioctl_close(d->sioctl.hdl);
		d->sioctl.hdl = NULL;
	}

	/* update parameters */
	d->mode = mode;
	d->par.bits = par.bits;
	d->par.bps = par.bps;
	d->par.sig = par.sig;
	d->par.le = par.le;
	d->par.msb = par.msb;
	if (d->mode & SIO_PLAY)
		d->pchan = par.pchan;
	if (d->mode & SIO_REC)
		d->rchan = par.rchan;

	d->sio.hdl = hdl;
	d->sioctl.hdl = ctlhdl;
	d->sio.file = file_new(&dev_sio_ops, d, "dev", sio_nfds(hdl));
	if (d->sioctl.hdl) {
		d->sioctl.file = file_new(&dev_sioctl_ops, d, "mix",
		    sioctl_nfds(ctlhdl));
	}
	sio_onmove(hdl, dev_sio_onmove, d);
	return 1;
bad_close:
	sio_close(hdl);
	if (ctlhdl)
		sioctl_close(ctlhdl);
	return 0;
}

void
dev_sio_close(struct dev *d)
{
	dev_sioctl_close(d);
#ifdef DEBUG
	if (log_level >= 3) {
		dev_log(d);
		log_puts(": closed\n");
	}
#endif
	timo_del(&d->sio.watchdog);
	file_del(d->sio.file);
	sio_close(d->sio.hdl);
	if (d->sioctl.hdl) {
		file_del(d->sioctl.file);
		sioctl_close(d->sioctl.hdl);
		d->sioctl.hdl = NULL;
	}
	d->alt_num = -1;
}

void
dev_sio_start(struct dev *d)
{
	if (!sio_start(d->sio.hdl)) {
		if (log_level >= 1) {
			dev_log(d);
			log_puts(": failed to start device\n");
		}
		return;
	}
	if (d->mode & MODE_PLAY) {
		d->sio.cstate = DEV_SIO_CYCLE;
		d->sio.todo = 0;
	} else {
		d->sio.cstate = DEV_SIO_READ;
		d->sio.todo = d->round * d->rchan * d->par.bps;
	}
#ifdef DEBUG
	d->sio.pused = 0;
	d->sio.rused = 0;
	d->sio.sum_utime = 0;
	d->sio.sum_wtime = 0;
	d->sio.wtime = file_wtime;
	d->sio.utime = file_utime;
	if (log_level >= 3) {
		dev_log(d);
		log_puts(": started\n");
	}
#endif
	timo_add(&d->sio.watchdog, WATCHDOG_USEC);
}

void
dev_sio_stop(struct dev *d)
{
	if (!sio_eof(d->sio.hdl) && !sio_stop(d->sio.hdl)) {
		if (log_level >= 1) {
			dev_log(d);
			log_puts(": failed to stop device\n");
		}
		return;
	}
#ifdef DEBUG
	if (log_level >= 3) {
		dev_log(d);
		log_puts(": stopped, load avg = ");
		log_puti(d->sio.sum_utime / 1000);
		log_puts(" / ");
		log_puti(d->sio.sum_wtime / 1000);
		log_puts("\n");
	}
#endif
	timo_del(&d->sio.watchdog);
}

int
dev_sio_pollfd(void *arg, struct pollfd *pfd)
{
	struct dev *d = arg;
	int events;

	events = (d->sio.cstate == DEV_SIO_READ) ? POLLIN : POLLOUT;
	return sio_pollfd(d->sio.hdl, pfd, events);
}

int
dev_sio_revents(void *arg, struct pollfd *pfd)
{
	struct dev *d = arg;
	int events;

	events = sio_revents(d->sio.hdl, pfd);
#ifdef DEBUG
	d->sio.events = events;
#endif
	return events;
}

void
dev_sio_run(void *arg)
{
	struct dev *d = arg;
	unsigned char *data, *base;
	unsigned int n;

	/*
	 * sio_read() and sio_write() would block at the end of the
	 * cycle so we *must* return and restart poll()'ing. Otherwise
	 * we may trigger dev_cycle() which would make all clients
	 * underrun (ex, on a play-only device)
	 */
	for (;;) {
		if (d->pstate != DEV_RUN)
			return;
		switch (d->sio.cstate) {
		case DEV_SIO_READ:
#ifdef DEBUG
			if (!(d->sio.events & POLLIN)) {
				dev_log(d);
				log_puts(": recording, but POLLIN not set\n");
				panic();
			}
			if (d->sio.todo == 0) {
				dev_log(d);
				log_puts(": can't read data\n");
				panic();
			}
			if (d->prime > 0) {
				dev_log(d);
				log_puts(": unexpected data\n");
				panic();
			}
#endif
			base = d->decbuf ? d->decbuf : (unsigned char *)d->rbuf;
			data = base +
			    d->rchan * d->round * d->par.bps -
			    d->sio.todo;
			n = sio_read(d->sio.hdl, data, d->sio.todo);
			d->sio.todo -= n;
#ifdef DEBUG
			if (log_level >= 4) {
				dev_log(d);
				log_puts(": read ");
				log_putu(n);
				log_puts(": bytes, todo ");
				log_putu(d->sio.todo);
				log_puts("/");
				log_putu(d->round * d->rchan * d->par.bps);
				log_puts("\n");
			}
#endif
			if (d->sio.todo > 0)
				return;
#ifdef DEBUG
			d->sio.rused -= d->round;
			if (log_level >= 2) {
				if (d->sio.rused >= d->round) {
					dev_log(d);
					log_puts(": rec hw xrun, rused = ");
					log_puti(d->sio.rused);
					log_puts("/");
					log_puti(d->bufsz);
					log_puts("\n");
				}
				if (d->sio.rused < 0 ||
				    d->sio.rused >= d->bufsz) {
					dev_log(d);
					log_puts(": out of bounds rused = ");
					log_puti(d->sio.rused);
					log_puts("/");
					log_puti(d->bufsz);
					log_puts("\n");
				}
			}
#endif
			d->sio.cstate = DEV_SIO_CYCLE;
			break;
		case DEV_SIO_CYCLE:
			timo_del(&d->sio.watchdog);
			timo_add(&d->sio.watchdog, WATCHDOG_USEC);

#ifdef DEBUG
			/*
			 * check that we're called at cycle boundary:
			 * either after a recorded block, or when POLLOUT is
			 * raised
			 */
			if (!((d->mode & MODE_REC) && d->prime == 0) &&
			    !(d->sio.events & POLLOUT)) {
				dev_log(d);
				log_puts(": cycle not at block boundary\n");
				panic();
			}
#endif
			dev_cycle(d);
			if (d->mode & MODE_PLAY) {
				d->sio.cstate = DEV_SIO_WRITE;
				d->sio.todo = d->round * d->pchan * d->par.bps;
				break;
			} else {
				d->sio.cstate = DEV_SIO_READ;
				d->sio.todo = d->round * d->rchan * d->par.bps;
				return;
			}
		case DEV_SIO_WRITE:
#ifdef DEBUG
			if (d->sio.todo == 0) {
				dev_log(d);
				log_puts(": can't write data\n");
				panic();
			}
#endif
			base = d->encbuf ? d->encbuf : (unsigned char *)DEV_PBUF(d);
			data = base +
			    d->pchan * d->round * d->par.bps -
			    d->sio.todo;
			n = sio_write(d->sio.hdl, data, d->sio.todo);
			d->sio.todo -= n;
#ifdef DEBUG
			if (log_level >= 4) {
				dev_log(d);
				log_puts(": wrote ");
				log_putu(n);
				log_puts(" bytes, todo ");
				log_putu(d->sio.todo);
				log_puts("/");
				log_putu(d->round * d->pchan * d->par.bps);
				log_puts("\n");
			}
#endif
			if (d->sio.todo > 0)
				return;
#ifdef DEBUG
			d->sio.pused += d->round;
			if (log_level >= 2) {
				if (d->prime == 0 &&
				    d->sio.pused <= d->bufsz - d->round) {
					dev_log(d);
					log_puts(": play hw xrun, pused = ");
					log_puti(d->sio.pused);
					log_puts("/");
					log_puti(d->bufsz);
					log_puts("\n");
				}
				if (d->sio.pused < 0 ||
				    d->sio.pused > d->bufsz) {
					/* device driver or libsndio bug */
					dev_log(d);
					log_puts(": out of bounds pused = ");
					log_puti(d->sio.pused);
					log_puts("/");
					log_puti(d->bufsz);
					log_puts("\n");
				}
			}
#endif
			d->poffs += d->round;
			if (d->poffs == d->psize)
				d->poffs = 0;
			if ((d->mode & MODE_REC) && d->prime == 0) {
				d->sio.cstate = DEV_SIO_READ;
				d->sio.todo = d->round * d->rchan * d->par.bps;
			} else
				d->sio.cstate = DEV_SIO_CYCLE;
			return;
		}
	}
}

void
dev_sio_hup(void *arg)
{
	struct dev *d = arg;

#ifdef DEBUG
	if (log_level >= 2) {
		dev_log(d);
		log_puts(": disconnected\n");
	}
#endif
	if (!dev_reopen(d))
		dev_abort(d);
}
