/*	$OpenBSD: dev.c,v 1.27 2009/07/25 10:52:18 ratchov Exp $	*/
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "abuf.h"
#include "aproc.h"
#include "conf.h"
#include "dev.h"
#include "pipe.h"
#include "safile.h"
#include "midi.h"

unsigned dev_bufsz, dev_round, dev_rate;
struct aparams dev_ipar, dev_opar;
struct aproc *dev_mix, *dev_sub, *dev_rec, *dev_play;
struct aproc *dev_midi;

/*
 * Create a MIDI thru box as the MIDI end of the device
 */
void
dev_thruinit(void)
{
	dev_midi = thru_new("thru");
	dev_midi->refs++;
}

/*
 * Terminate the MIDI thru box
 */
void
dev_thrudone(void)
{
	struct file *f;

 restart:
	LIST_FOREACH(f, &file_list, entry) {
		if (f->rproc && aproc_depend(dev_midi, f->rproc)) {
			file_eof(f);
			goto restart;
		}
	}
	while (!LIST_EMPTY(&dev_midi->ibuflist)) {
		if (!file_poll())
			break;
	}
	dev_midi->refs--;
	aproc_del(dev_midi);
	dev_midi = NULL;
	while (file_poll())
		; /* nothing */
}

/*
 * Attach a bi-directional MIDI stream to the MIDI device
 */
void
dev_midiattach(struct abuf *ibuf, struct abuf *obuf)
{
	if (ibuf)
		aproc_setin(dev_midi, ibuf);
	if (obuf) {
		aproc_setout(dev_midi, obuf);
		if (ibuf) {
			ibuf->duplex = obuf;
			obuf->duplex = ibuf;
		}
	}
}

/*
 * Same as dev_init(), but create a fake device that records what is
 * played.
 */
void
dev_loopinit(struct aparams *dipar, struct aparams *dopar, unsigned bufsz)
{
	struct abuf *buf;
	struct aparams par;
	unsigned cmin, cmax, rate;

	cmin = (dipar->cmin < dopar->cmin) ? dipar->cmin : dopar->cmin;
	cmax = (dipar->cmax > dopar->cmax) ? dipar->cmax : dopar->cmax;
	rate = (dipar->rate > dopar->rate) ? dipar->rate : dopar->rate;
	aparams_init(&par, cmin, cmax, rate);
	dev_ipar = par;
	dev_opar = par;
	dev_round = (bufsz + 1) / 2;
	dev_bufsz = dev_round * 2;
	dev_rate  = rate;
	dev_rec = NULL;
	dev_play = NULL;

	buf = abuf_new(dev_bufsz, &par);
	dev_mix = mix_new("mix", dev_bufsz);
	dev_mix->refs++;
	dev_sub = sub_new("sub", dev_bufsz);
	dev_sub->refs++;
	aproc_setout(dev_mix, buf);
	aproc_setin(dev_sub, buf);

	dev_mix->u.mix.flags |= MIX_AUTOQUIT;
	dev_sub->u.sub.flags |= SUB_AUTOQUIT;

	*dipar = dev_ipar;
	*dopar = dev_opar;
}

/*
 * Same as dev_done(), but destroy a loopback device.
 */
void
dev_loopdone(void)
{
	struct file *f;

	DPRINTF("dev_loopdone:\n");

	dev_sub->refs--;
	dev_sub = NULL;
	dev_mix->refs--;
	dev_mix = NULL;
	/*
	 * Generate EOF on all inputs.
	 */
 restart:
	LIST_FOREACH(f, &file_list, entry) {
		if (f->rproc != NULL) {
			file_eof(f);
			goto restart;
		}
	}
	while (file_poll())
		; /* nothing */
}

unsigned
dev_roundof(unsigned newrate)
{
	return (dev_round * newrate + dev_rate / 2) / dev_rate;
}

/*
 * Open the device with the given hardware parameters and create a mixer
 * and a multiplexer connected to it with all necessary conversions
 * setup.
 */
int
dev_init(char *devpath,
    struct aparams *dipar, struct aparams *dopar, unsigned bufsz)
{
	struct file *f;
	struct aparams ipar, opar;
	struct aproc *conv;
	struct abuf *buf;
	unsigned nfr, ibufsz, obufsz;

	/*
	 * Ask for 1/4 of the buffer for the kernel ring and
	 * limit the block size to 1/4 of the requested buffer.
	 */
	dev_bufsz = (bufsz + 3) / 4;
	dev_round = (bufsz + 3) / 4;
	f = (struct file *)safile_new(&safile_ops, devpath,
	    dipar, dopar, &dev_bufsz, &dev_round);
	if (f == NULL)
		return 0;
	if (dipar) {
#ifdef DEBUG
		if (debug_level > 0) {
			fprintf(stderr, "dev_init: hw recording ");
			aparams_print(dipar);
			fprintf(stderr, "\n");
		}
#endif
		dev_rate = dipar->rate;
	}
	if (dopar) {
#ifdef DEBUG
		if (debug_level > 0) {
			fprintf(stderr, "dev_init: hw playing ");
			aparams_print(dopar);
			fprintf(stderr, "\n");
		}
#endif
		dev_rate = dopar->rate;
	}
	ibufsz = obufsz = dev_bufsz;
	bufsz = (bufsz > dev_bufsz) ? bufsz - dev_bufsz : 0;

	/*
	 * Use 1/8 of the buffer for the mixer/converters.  Since we
	 * already consumed 1/4 for the device, bufsz represents the
	 * remaining 3/4. So 1/8 is 1/6 of 3/4.
	 */
	nfr = (bufsz + 5) / 6;
	nfr += dev_round - 1;
	nfr -= nfr % dev_round;
	if (nfr == 0)
		nfr = dev_round;

	/*
	 * Create record chain.
	 */
	if (dipar) {
		aparams_init(&ipar, dipar->cmin, dipar->cmax, dipar->rate);
		/*
		 * Create the read end.
		 */
		dev_rec = rpipe_new(f);
		dev_rec->refs++;
		buf = abuf_new(nfr, dipar);
		aproc_setout(dev_rec, buf);
		ibufsz += nfr;

		/*
		 * Append a converter, if needed.
		 */
		if (!aparams_eqenc(dipar, &ipar)) {
			conv = dec_new("subin", dipar);
			aproc_setin(conv, buf);
			buf = abuf_new(nfr, &ipar);
			aproc_setout(conv, buf);
			ibufsz += nfr;
		}
		dev_ipar = ipar;

		/*
		 * Append a "sub" to which clients will connect.
		 */
		dev_sub = sub_new("sub", nfr);
		dev_sub->refs++;
		aproc_setin(dev_sub, buf);
	} else {
		dev_rec = NULL;
		dev_sub = NULL;
	}

	/*
	 * Create play chain.
	 */
	if (dopar) {
		aparams_init(&opar, dopar->cmin, dopar->cmax, dopar->rate);
		/*
		 * Create the write end.
		 */
		dev_play = wpipe_new(f);
		dev_play->refs++;
		buf = abuf_new(nfr, dopar);
		aproc_setin(dev_play, buf);
		obufsz += nfr;

		/*
		 * Append a converter, if needed.
		 */
		if (!aparams_eqenc(&opar, dopar)) {
			conv = enc_new("mixout", dopar);
			aproc_setout(conv, buf);
			buf = abuf_new(nfr, &opar);
			aproc_setin(conv, buf);
			obufsz += nfr;
		}
		dev_opar = opar;

		/*
		 * Append a "mix" to which clients will connect.
		 */
		dev_mix = mix_new("mix", nfr);
		dev_mix->refs++;
		aproc_setout(dev_mix, buf);
	} else {
		dev_play = NULL;
		dev_mix = NULL;
	}
	dev_bufsz = (dopar) ? obufsz : ibufsz;
	DPRINTF("dev_init: using %u fpb\n", dev_bufsz);
	dev_start();
	return 1;
}

/*
 * Cleanly stop and drain everything and close the device
 * once both play chain and record chain are gone.
 */
void
dev_done(void)
{
	struct file *f;

	DPRINTF("dev_done: dev_mix = %p, dev_sub = %p\n", dev_mix, dev_sub);
	if (dev_mix) {
		dev_mix->refs--;
		dev_mix->u.mix.flags |= MIX_AUTOQUIT;
		dev_mix = NULL;
		/*
		 * Generate EOF on all inputs (but not the device), and
		 * put the mixer in ``autoquit'' state, so once buffers
		 * are drained the mixer will terminate and shutdown the
		 * write-end of the device.
		 *
		 * NOTE: since file_eof() can destroy the file and
		 * reorder the file_list, we have to restart the loop
		 * after each call to file_eof().
		 */
	restart:
		LIST_FOREACH(f, &file_list, entry) {
			if (f->rproc != NULL && f->rproc != dev_rec) {
				file_eof(f);
				goto restart;
			}
		}

		/*
		 * Wait for play chain to terminate.
		 */
		if (dev_play) {
			while (!LIST_EMPTY(&dev_play->ibuflist)) {
				if (!file_poll())
					break;
			}
			dev_play->refs--;
			aproc_del(dev_play);
			dev_play = NULL;
		}
	}
	if (dev_sub) {
		dev_sub->refs--;
		dev_sub->u.sub.flags |= SUB_AUTOQUIT;
		dev_sub = NULL;
		/*
		 * Same as above, but for the record chain: generate eof
		 * on the read-end of the device and wait record buffers
		 * to disappear.  We must stop the device first, because
		 * play-end will underrun (and xrun correction code will
		 * insert silence on the record-end of the device).
		 */
		if (dev_rec) {
			dev_stop();
			if (dev_rec->u.io.file)
				file_eof(dev_rec->u.io.file);
			while (!LIST_EMPTY(&dev_rec->obuflist)) {
				if (!file_poll())
					break;
			}
			dev_rec->refs--;
			aproc_del(dev_rec);
			dev_rec = NULL;
		}
	}
}

/*
 * Start the (paused) device. By default it's paused.
 */
void
dev_start(void)
{
	struct file *f;

	if (dev_mix)
		dev_mix->u.mix.flags |= MIX_DROP;
	if (dev_sub)
		dev_sub->u.sub.flags |= SUB_DROP;
	if (dev_play && dev_play->u.io.file) {
		f = dev_play->u.io.file;
		f->ops->start(f);
	} else if (dev_rec && dev_rec->u.io.file) {
		f = dev_rec->u.io.file;
		f->ops->start(f);
	}
}

/*
 * Pause the device.
 */
void
dev_stop(void)
{
	struct file *f;

	if (dev_play && dev_play->u.io.file) {
		f = dev_play->u.io.file;
		f->ops->stop(f);
	} else if (dev_rec && dev_rec->u.io.file) {
		f = dev_rec->u.io.file;
		f->ops->stop(f);
	}
	if (dev_mix)
		dev_mix->u.mix.flags &= ~MIX_DROP;
	if (dev_sub)
		dev_sub->u.sub.flags &= ~SUB_DROP;
}

/*
 * Find the end points connected to the mix/sub.
 */
int
dev_getep(struct abuf **sibuf, struct abuf **sobuf)
{
	struct abuf *ibuf, *obuf;

	if (sibuf) {
		ibuf = *sibuf;
		for (;;) {
			if (!ibuf || !ibuf->rproc) {
				DPRINTF("dev_getep: reader desappeared\n");
				return 0;
			}
			if (ibuf->rproc == dev_mix)
				break;
			ibuf = LIST_FIRST(&ibuf->rproc->obuflist);
		}
		*sibuf = ibuf;
	}
	if (sobuf) {
		obuf = *sobuf;
		for (;;) {
			if (!obuf || !obuf->wproc) {
				DPRINTF("dev_getep: writer desappeared\n");
				return 0;
			}
			if (obuf->wproc == dev_sub)
				break;
			obuf = LIST_FIRST(&obuf->wproc->ibuflist);
		}
		*sobuf = obuf;
	}
	return 1;
}

/*
 * Sync play buffer to rec buffer (for instance when one of
 * them underruns/overruns).
 */
void
dev_sync(struct abuf *ibuf, struct abuf *obuf)
{
	struct abuf *pbuf, *rbuf;
	int delta;

	if (!dev_mix || !dev_sub)
		return;
	pbuf = LIST_FIRST(&dev_mix->obuflist);
	if (!pbuf)
		return;
	rbuf = LIST_FIRST(&dev_sub->ibuflist);
	if (!rbuf)
		return;
	if (!dev_getep(&ibuf, &obuf))
		return;

	/*
	 * Calculate delta, the number of frames the play chain is ahead
	 * of the record chain. It's necessary to schedule silences (or
	 * drops) in order to start playback and record in sync.
	 */
	delta =
	    rbuf->bpf * (pbuf->abspos + pbuf->used) -
	    pbuf->bpf *  rbuf->abspos;
	delta /= pbuf->bpf * rbuf->bpf;
	DPRINTF("dev_sync: delta = %d, ppos = %u, pused = %u, rpos = %u\n",
	    delta, pbuf->abspos, pbuf->used, rbuf->abspos);

	if (delta > 0) {
		/*
		 * If the play chain is ahead (most cases) drop some of
		 * the recorded input, to get both in sync.
		 */
		obuf->drop += delta * obuf->bpf;
		abuf_ipos(obuf, -delta);
	} else if (delta < 0) {
		/*
		 * If record chain is ahead (should never happen,
		 * right?) then insert silence to play.
		 */
		ibuf->silence += -delta * ibuf->bpf;
		abuf_opos(ibuf, delta);
	} else
		DPRINTF("dev_sync: nothing to do\n");
}

/*
 * Attach the given input and output buffers to the mixer and the
 * multiplexer respectively. The operation is done synchronously, so
 * both buffers enter in sync. If buffers do not match play
 * and rec.
 */
void
dev_attach(char *name,
    struct abuf *ibuf, struct aparams *sipar, unsigned underrun,
    struct abuf *obuf, struct aparams *sopar, unsigned overrun, int vol)
{
	struct abuf *pbuf = NULL, *rbuf = NULL;
	struct aparams ipar, opar;
	struct aproc *conv;
	unsigned round, nblk;

	if (ibuf) {
		ipar = *sipar;
		pbuf = LIST_FIRST(&dev_mix->obuflist);
		nblk = (dev_bufsz / dev_round + 3) / 4;
		round = dev_roundof(ipar.rate);
		if (!aparams_eqenc(&ipar, &dev_opar)) {
			conv = dec_new(name, &ipar);
			ipar.bps = dev_opar.bps;
			ipar.bits = dev_opar.bits;
			ipar.sig = dev_opar.sig;
			ipar.le = dev_opar.le;
			ipar.msb = dev_opar.msb;
			aproc_setin(conv, ibuf);
			ibuf = abuf_new(nblk * round, &ipar);
			aproc_setout(conv, ibuf);
		}
		if (!aparams_subset(&ipar, &dev_opar)) {
			conv = cmap_new(name, &ipar, &dev_opar);
			ipar.cmin = dev_opar.cmin;
			ipar.cmax = dev_opar.cmax;
			aproc_setin(conv, ibuf);
			ibuf = abuf_new(nblk * round, &ipar);
			aproc_setout(conv, ibuf);
		}
		if (!aparams_eqrate(&ipar, &dev_opar)) {
			conv = resamp_new(name, round, dev_round);
			ipar.rate = dev_opar.rate;
			round = dev_round;
			aproc_setin(conv, ibuf);
			ibuf = abuf_new(nblk * round, &ipar);
			aproc_setout(conv, ibuf);
		}
		aproc_setin(dev_mix, ibuf);
		abuf_opos(ibuf, -dev_mix->u.mix.lat);
		ibuf->xrun = underrun;
		ibuf->mixmaxweight = vol;
		mix_setmaster(dev_mix);
	}
	if (obuf) {
		opar = *sopar;
		rbuf = LIST_FIRST(&dev_sub->ibuflist);
		round = dev_roundof(opar.rate);
		nblk = (dev_bufsz / dev_round + 3) / 4;
		if (!aparams_eqenc(&opar, &dev_ipar)) {
			conv = enc_new(name, &opar);
			opar.bps = dev_ipar.bps;
			opar.bits = dev_ipar.bits;
			opar.sig = dev_ipar.sig;
			opar.le = dev_ipar.le;
			opar.msb = dev_ipar.msb;
			aproc_setout(conv, obuf);
			obuf = abuf_new(nblk * round, &opar);
			aproc_setin(conv, obuf);
		}
		if (!aparams_subset(&opar, &dev_ipar)) {
			conv = cmap_new(name, &dev_ipar, &opar);
			opar.cmin = dev_ipar.cmin;
			opar.cmax = dev_ipar.cmax;
			aproc_setout(conv, obuf);
			obuf = abuf_new(nblk * round, &opar);
			aproc_setin(conv, obuf);
		}
		if (!aparams_eqrate(&opar, &dev_ipar)) {
			conv = resamp_new(name, dev_round, round);
			opar.rate = dev_ipar.rate;
			round = dev_round;
			aproc_setout(conv, obuf);
			obuf = abuf_new(nblk * round, &opar);
			aproc_setin(conv, obuf);
		}
		aproc_setout(dev_sub, obuf);
		abuf_ipos(obuf, -dev_sub->u.sub.lat);
		obuf->xrun = overrun;
	}

	/*
	 * Sync play to record.
	 */
	if (ibuf && obuf) {
		ibuf->duplex = obuf;
		obuf->duplex = ibuf;
		dev_sync(ibuf, obuf);
	}
}

/*
 * Change the playback volume of the given stream.
 */
void
dev_setvol(struct abuf *ibuf, int vol)
{
	DPRINTF("dev_setvol: %p\n", ibuf);
	if (!dev_getep(&ibuf, NULL)) {
		DPRINTF("dev_setvol: not connected yet\n");
		return;
	}
	ibuf->mixvol = vol;
	DPRINTF("dev_setvol: %p -> %d\n", ibuf, vol);
}

/*
 * Clear buffers of the play and record chains so that when the device
 * is started, playback and record start in sync.
 */
void
dev_clear(void)
{
	struct abuf *buf;

	if (dev_mix) {
		if (!LIST_EMPTY(&dev_mix->ibuflist)) {
			fprintf(stderr, "dev_clear: mixer not idle\n");
			abort();
		}
		buf = LIST_FIRST(&dev_mix->obuflist);
		while (buf) {
			abuf_clear(buf);
			buf = LIST_FIRST(&buf->rproc->obuflist);
		}
		mix_clear(dev_mix);
	}
	if (dev_sub) {
		if (!LIST_EMPTY(&dev_sub->obuflist)) {
			fprintf(stderr, "dev_suspend: demux not idle\n");
			abort();
		}
		buf = LIST_FIRST(&dev_sub->ibuflist);
		while (buf) {
			abuf_clear(buf);
			buf = LIST_FIRST(&buf->wproc->ibuflist);
		}
		sub_clear(dev_sub);
	}
}
