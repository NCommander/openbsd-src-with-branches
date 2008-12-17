/*	$OpenBSD: sun.c,v 1.7 2008/11/20 16:31:26 ratchov Exp $	*/
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
/*
 * TODO:
 *
 * remove filling code from sun_write() and create sun_fill()
 *
 * allow block size to be set
 *
 * call hdl->cb_pos() from sun_read() and sun_write(), or better:
 * implement generic blocking sio_read() and sio_write() with poll(2)
 * and use non-blocking sio_ops only
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sndio_priv.h"

struct sun_hdl {
	struct sio_hdl sa;
	int fd;
	int filling;
	unsigned ibpf, obpf;		/* bytes per frame */
	unsigned ibytes, obytes;	/* bytes the hw transfered */
	unsigned ierr, oerr;		/* frames the hw dropped */
	int offset;			/* frames play is ahead of record */
	int idelta, odelta;		/* position reported to client */
	int mix_fd, mix_index;		/* /dev/mixerN stuff */
	int voltodo;			/* 1 if vol initialization pending */
	unsigned curvol;
};

void sun_close(struct sio_hdl *);
int sun_start(struct sio_hdl *);
int sun_stop(struct sio_hdl *);
int sun_setpar(struct sio_hdl *, struct sio_par *);
int sun_getpar(struct sio_hdl *, struct sio_par *);
int sun_getcap(struct sio_hdl *, struct sio_cap *);
size_t sun_read(struct sio_hdl *, void *, size_t);
size_t sun_write(struct sio_hdl *, void *, size_t);
int sun_pollfd(struct sio_hdl *, struct pollfd *, int);
int sun_revents(struct sio_hdl *, struct pollfd *);
int sun_setvol(struct sio_hdl *, unsigned);
void sun_getvol(struct sio_hdl *);

struct sio_ops sun_ops = {
	sun_close,
	sun_setpar,
	sun_getpar,
	sun_getcap,
	sun_write,
	sun_read,
	sun_start,
	sun_stop,
	sun_pollfd,
	sun_revents,
	sun_setvol,
	sun_getvol
};

/*
 * prefered controls for the volume knob, in reverse order of preference
 */
struct sun_pref {
	char *cls, *dev;
} sun_vols[] = {
	{ AudioCoutputs, AudioNmaster },
	{ AudioCoutputs, AudioNoutput },
	{ AudioCoutputs, AudioNdac },
	{ AudioCinputs, AudioNdac },
	{ NULL, NULL}
};

/*
 * convert sun encoding to sio_par encoding
 */
void
sun_infotoenc(struct audio_prinfo *ai, struct sio_par *par)
{
	par->msb = 1;
	par->bits = ai->precision;
	par->bps = SIO_BPS(par->bits);
	switch (ai->encoding) {
	case AUDIO_ENCODING_SLINEAR_LE:
		par->le = 1;
		par->sig = 1;
		break;
	case AUDIO_ENCODING_SLINEAR_BE:
		par->le = 0;
		par->sig = 1;
		break;
	case AUDIO_ENCODING_ULINEAR_LE:
		par->le = 1;
		par->sig = 0;
		break;
	case AUDIO_ENCODING_ULINEAR_BE:
		par->le = 0;
		par->sig = 0;
		break;
	case AUDIO_ENCODING_SLINEAR:
		par->le = SIO_LE_NATIVE;
		par->sig = 1;
		break;
	case AUDIO_ENCODING_ULINEAR:
		par->le = SIO_LE_NATIVE;
		par->sig = 0;
		break;
	default:
		fprintf(stderr, "sun_infotoenc: unsupported encoding\n");
		exit(1);
	}
}

/*
 * convert sio_par encoding to sun encoding
 */
void
sun_enctoinfo(struct audio_prinfo *ai, struct sio_par *par)
{
	if (par->le && par->sig) {
		ai->encoding = AUDIO_ENCODING_SLINEAR_LE;
	} else if (!par->le && par->sig) {
		ai->encoding = AUDIO_ENCODING_SLINEAR_BE;
	} else if (par->le && !par->sig) {
		ai->encoding = AUDIO_ENCODING_ULINEAR_LE;
	} else {
		ai->encoding = AUDIO_ENCODING_ULINEAR_BE;
	}
	ai->precision = par->bits;
}

/*
 * try to set the device to the given parameters and check that the
 * device can use them; retrun 1 on success, 0 on failure or error
 */
int
sun_tryinfo(struct sun_hdl *hdl, struct sio_enc *enc, 
    unsigned pchan, unsigned rchan, unsigned rate)
{
	struct audio_info aui;	
	
	AUDIO_INITINFO(&aui);
	if (enc) {
		if (enc->le && enc->sig) {
			aui.play.encoding = AUDIO_ENCODING_SLINEAR_LE;
			aui.record.encoding = AUDIO_ENCODING_SLINEAR_LE;
		} else if (!enc->le && enc->sig) {
			aui.play.encoding = AUDIO_ENCODING_SLINEAR_BE;
			aui.record.encoding = AUDIO_ENCODING_SLINEAR_BE;
		} else if (enc->le && !enc->sig) {
			aui.play.encoding = AUDIO_ENCODING_ULINEAR_LE;
			aui.record.encoding = AUDIO_ENCODING_ULINEAR_LE;
		} else {
			aui.play.encoding = AUDIO_ENCODING_ULINEAR_BE;
			aui.record.encoding = AUDIO_ENCODING_ULINEAR_BE;
		}
		aui.play.precision = enc->bits;
	}
	if (pchan)
		aui.play.channels = pchan;
	if (rchan)
		aui.record.channels = rchan;
	if (rate) {
		if (hdl->sa.mode & SIO_PLAY)
			aui.play.sample_rate = rate;
		if (hdl->sa.mode & SIO_REC)
			aui.record.sample_rate = rate;
	}
	if (ioctl(hdl->fd, AUDIO_SETINFO, &aui) < 0) {
		if (errno == EINVAL)
			return 0;
		perror("sun_tryinfo: setinfo");
		hdl->sa.eof = 1;
		return 0;
	}
	if (ioctl(hdl->fd, AUDIO_GETINFO, &aui) < 0) {
		perror("sun_tryinfo: getinfo");
		hdl->sa.eof = 1;
		return 0;
	}
	if (pchan && aui.play.channels != pchan)
		return 0;
	if (rchan && aui.record.channels != rchan)
		return 0;
	if (rate) {
		if ((hdl->sa.mode & SIO_PLAY) &&
		    (aui.play.sample_rate != rate))
			return 0;
		if ((hdl->sa.mode & SIO_REC) &&
		    (aui.record.sample_rate != rate))
			return 0;
	}
	return 1;
}

/*
 * guess device capabilities
 */
int
sun_getcap(struct sio_hdl *sh, struct sio_cap *cap)
{
#define NCHANS (sizeof(chans) / sizeof(chans[0]))
#define NRATES (sizeof(rates) / sizeof(rates[0]))
	static unsigned chans[] = { 
		1, 2, 4, 6, 8, 10, 12
	};
	static unsigned rates[] = { 
		8000, 11025, 12000, 16000, 22050, 24000,
		32000, 44100, 48000, 64000, 88200, 96000
	};
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	struct sio_par savepar;
	struct audio_encoding ae;
	unsigned nenc = 0, nconf = 0;
	unsigned enc_map = 0, rchan_map = 0, pchan_map = 0, rate_map = 0;
	unsigned i, j, map;

	if (!sun_getpar(&hdl->sa, &savepar))
		return 0;

	/*
	 * fill encoding list
	 */
	for (ae.index = 0; nenc < SIO_NENC; ae.index++) {
		if (ioctl(hdl->fd, AUDIO_GETENC, &ae) < 0) {
			if (errno == EINVAL)
				break;
			perror("sun_getcap: getenc");
			hdl->sa.eof = 1;
			return 0;
		}
		if (ae.flags & AUDIO_ENCODINGFLAG_EMULATED)
			continue;
		if (ae.encoding == AUDIO_ENCODING_SLINEAR_LE) {
			cap->enc[nenc].le = 1;
			cap->enc[nenc].sig = 1;
		} else if (ae.encoding == AUDIO_ENCODING_SLINEAR_BE) {
			cap->enc[nenc].le = 0;
			cap->enc[nenc].sig = 1;
		} else if (ae.encoding == AUDIO_ENCODING_ULINEAR_LE) {
			cap->enc[nenc].le = 1;
			cap->enc[nenc].sig = 0;
		} else if (ae.encoding == AUDIO_ENCODING_ULINEAR_BE) {
			cap->enc[nenc].le = 0;
			cap->enc[nenc].sig = 0;
		} else if (ae.encoding == AUDIO_ENCODING_SLINEAR) {
			cap->enc[nenc].le = SIO_LE_NATIVE;
			cap->enc[nenc].sig = 1;
		} else if (ae.encoding == AUDIO_ENCODING_ULINEAR) {
			cap->enc[nenc].le = SIO_LE_NATIVE;
			cap->enc[nenc].sig = 0;
		} else {
			/* unsipported encoding */
			continue;
		}
		cap->enc[nenc].bits = ae.precision;
		cap->enc[nenc].bps = ae.precision / 8;
		cap->enc[nenc].msb = 0;
		enc_map |= (1 << nenc);
		nenc++;
	}

	/*
	 * fill channels
	 *
	 * for now we're lucky: all kernel devices assume that the
	 * number of channels and the encoding are independent so we can
	 * use the current encoding and try various channels.
	 */
	if (hdl->sa.mode & SIO_PLAY) {
		memcpy(&cap->pchan, chans, NCHANS * sizeof(unsigned));
		for (i = 0; i < NCHANS; i++) {
			if (sun_tryinfo(hdl, NULL, chans[i], 0, 0))
				pchan_map |= (1 << i);
		}
	}
	if (hdl->sa.mode & SIO_REC) {
		memcpy(&cap->rchan, chans, NCHANS * sizeof(unsigned));
		for (i = 0; i < NCHANS; i++) {
			if (sun_tryinfo(hdl, NULL, 0, chans[i], 0))
				rchan_map |= (1 << i);
		}
	}
	
	/*
	 * fill rates
	 *
	 * rates are not independent from other parameters (eg. on
	 * uaudio devices), so certain rates may not be allowed with
	 * certain encordings. We have to check rates for all encodings
	 */
	memcpy(&cap->rate, rates, NRATES * sizeof(unsigned));
	for (j = 0; j < nenc; j++) {
		if (nconf == SIO_NCONF)
			break;
		map = 0;
		for (i = 0; i < NRATES; i++) {
			if (sun_tryinfo(hdl, NULL, 0, 0, rates[i]))
				map |= (1 << i);
		}
		if (map != rate_map) {
			rate_map = map;
			cap->confs[nconf].enc = enc_map;
			cap->confs[nconf].pchan = pchan_map;
			cap->confs[nconf].rchan = rchan_map;
			cap->confs[nconf].rate = rate_map;
			nconf++;
		}
	}
	cap->nconf = nconf;
	if (!sun_setpar(&hdl->sa, &savepar))
		return 0;
	return 1;
#undef NCHANS
#undef NRATES
}

/*
 * initialize volume knob
 */
void
sun_initvol(struct sun_hdl *hdl)
{
	int i, fd, index = -1, last_pref = -1;
	struct sun_pref *p;
	struct stat sb;
	struct mixer_devinfo mi, cl;
	struct mixer_ctrl m;
	char path[PATH_MAX];

	if (fstat(hdl->fd, &sb) < 0)
		return;
	if (!S_ISCHR(sb.st_mode))
		return;
	snprintf(path, PATH_MAX, "/dev/mixer%d", sb.st_rdev & 0xf);
	fd = open(path, O_RDWR);	
	if (fd < 0) {
		fprintf(stderr, "%s: couldn't open mixer\n", path);
		return;
	}

	for (mi.index = 0; ; mi.index++) {
		if (ioctl(fd, AUDIO_MIXER_DEVINFO, &mi) < 0)
			break;
		if (mi.type == AUDIO_MIXER_CLASS || mi.prev != -1)
			continue;
		cl.index = mi.mixer_class;
		if (ioctl(fd, AUDIO_MIXER_DEVINFO, &cl) < 0)
			continue;
		/*
		 * find prefered input gain and output gain
		 */
		for (i = 0, p = sun_vols; p->cls != NULL; i++, p++) {
			if (strcmp(p->cls, cl.label.name) != 0 ||
			    strcmp(p->dev, mi.label.name) != 0)
				continue;
			if (last_pref < i) {
				index = mi.index;
				last_pref = i;
			}
			break;
		}
	}
	hdl->mix_fd = fd;
	hdl->mix_index = index;
	if (index >= 0) {
		m.dev = index;
		m.type = AUDIO_MIXER_VALUE;
		m.un.value.num_channels = 1;
		if (ioctl(hdl->mix_fd, AUDIO_MIXER_READ, &m) < 0) {
			fprintf(stderr, "sun_getvol: %d: failed to get volume\n", m.dev);
			hdl->sa.eof = 1;
			return;
		}
		hdl->curvol = m.un.value.level[0] / 2;
	} else 
		hdl->curvol = SIO_MAXVOL;
	return;
}

void
sun_getvol(struct sio_hdl *sh)
{
	struct sun_hdl *hdl = (struct sun_hdl *)sh;

	if (hdl->voltodo) {
		sun_initvol(hdl);
		hdl->voltodo = 0;
	}
	sio_onvol_cb(&hdl->sa, hdl->curvol);
}

int
sun_setvol(struct sio_hdl *sh, unsigned vol)
{
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	struct mixer_ctrl m;
	
	if (hdl->voltodo) {
		sun_initvol(hdl);
		hdl->voltodo = 0;
	}
	if (hdl->mix_fd == -1 || hdl->mix_index == -1)
		return 0;
	m.dev = hdl->mix_index;
	m.type = AUDIO_MIXER_VALUE;
	m.un.value.num_channels = 1;
	m.un.value.level[0] = 2 * vol;
	if (ioctl(hdl->mix_fd, AUDIO_MIXER_WRITE, &m) < 0) {
		fprintf(stderr, "sun_setvol: failed to set volume\n");
		hdl->sa.eof = 1;
		return 0;
	}
	hdl->curvol = vol;
	return 1;
}

struct sio_hdl *
sio_open_sun(char *path, unsigned mode, int nbio)
{
	int fd, flags, fullduplex;
	struct sun_hdl *hdl;
	struct audio_info aui;
	struct sio_par par;

	hdl = malloc(sizeof(struct sun_hdl));
	if (hdl == NULL)
		return NULL;
	sio_create(&hdl->sa, &sun_ops, mode, nbio);

	if (path == NULL)
		path = SIO_SUN_PATH;
	if (mode == (SIO_PLAY | SIO_REC))
		flags = O_RDWR;
	else 
		flags = (mode & SIO_PLAY) ? O_WRONLY : O_RDONLY;

	while ((fd = open(path, flags | O_NONBLOCK)) < 0) {
		if (errno == EINTR)
			continue;
		perror(path);
		goto bad_free;
	}
	hdl->fd = fd;
	hdl->voltodo = 1;

	/*
	 * If both play and record are requested then
	 * set full duplex mode.
	 */
	if (mode == (SIO_PLAY | SIO_REC)) {
		fullduplex = 1;
		if (ioctl(fd, AUDIO_SETFD, &fullduplex) < 0) {
			fprintf(stderr, "%s: can't set full-duplex\n", path);
			goto bad_close;
		}
	}
	hdl->fd = fd;
	AUDIO_INITINFO(&aui);
	if (hdl->sa.mode & SIO_PLAY)
		aui.play.encoding = AUDIO_ENCODING_SLINEAR;
	if (hdl->sa.mode & SIO_REC)
		aui.record.encoding = AUDIO_ENCODING_SLINEAR;
	if (ioctl(hdl->fd, AUDIO_SETINFO, &aui) < 0) {
		perror("sio_open_sun: setinfo");
		goto bad_close;
	}
	sio_initpar(&par);
	par.rate = 48000;
	par.sig = 1;
	par.bits = 16;
	par.appbufsz = 1200;
	if (!sio_setpar(&hdl->sa, &par))
		goto bad_close;
	return (struct sio_hdl *)hdl;
 bad_close:
	while (close(hdl->fd) < 0 && errno == EINTR)
		; /* retry */
 bad_free:
	free(hdl);
	return NULL;
}

void
sun_close(struct sio_hdl *sh)
{
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	int rc;
	do {
		rc = close(hdl->fd);
	} while (rc < 0 && errno == EINTR);
	free(hdl);
}

int
sun_start(struct sio_hdl *sh)
{
	struct sio_par par;
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	struct audio_info aui;	

	if (!sio_getpar(&hdl->sa, &par))
		return 0;
	hdl->obpf = par.pchan * par.bps;
	hdl->ibpf = par.rchan * par.bps;
	hdl->ibytes = 0;
	hdl->obytes = 0;
	hdl->ierr = 0;
	hdl->oerr = 0;
	hdl->offset = 0;
	hdl->idelta = 0;
	hdl->odelta = 0;

	if (hdl->sa.mode & SIO_PLAY) {
		/* 
		 * pause the device and let sun_write() trigger the
		 * start later, to avoid buffer underruns
		 */
		AUDIO_INITINFO(&aui);
		if (hdl->sa.mode & SIO_PLAY)
			aui.play.pause = 1;
		if (hdl->sa.mode & SIO_REC)
			aui.record.pause = 1;
		if (ioctl(hdl->fd, AUDIO_SETINFO, &aui) < 0) {
			perror("sun_start: setinfo2");
			hdl->sa.eof = 1;
			return 0;
		}
		hdl->filling = 1;
	} else {
		/*
		 * no play buffers to fill, start now!
		 */
		AUDIO_INITINFO(&aui);
		if (hdl->sa.mode & SIO_PLAY)
			aui.play.pause = 0;
		if (hdl->sa.mode & SIO_REC)
			aui.record.pause = 0;
		if (ioctl(hdl->fd, AUDIO_SETINFO, &aui) < 0) {
			perror("sun_start: setinfo");
			hdl->sa.eof = 1;
			return 0;
		}
		sio_onmove_cb(&hdl->sa, 0);
	}
	return 1;
}

int
sun_stop(struct sio_hdl *sh)
{
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	struct audio_info aui;	
	int mode;

	if (ioctl(hdl->fd, AUDIO_GETINFO, &aui) < 0) {
		perror("sun_start: setinfo1");
		hdl->sa.eof = 1;
		return 0;
	}
	mode = aui.mode;

	/*
	 * there's no way to drain the device without blocking, so just
	 * stop it until the kernel driver get fixed
	 */
	AUDIO_INITINFO(&aui);
	aui.mode = 0;
	if (ioctl(hdl->fd, AUDIO_SETINFO, &aui) < 0) {
		perror("sun_stop: setinfo1");
		hdl->sa.eof = 1;
		return 0;
	}
	AUDIO_INITINFO(&aui);
	aui.mode = mode;
	if (ioctl(hdl->fd, AUDIO_SETINFO, &aui) < 0) {
		perror("sun_stop: setinfo2");
		hdl->sa.eof = 1;
		return 0;
	}
	return 1;
}

int
sun_setpar(struct sio_hdl *sh, struct sio_par *par)
{
#define NRETRIES 8
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	struct audio_info aui;
	unsigned i, infr, ibpf, onfr, obpf;
	unsigned bufsz, round;

	/*
	 * first, set encoding, rate and channels
	 */
	AUDIO_INITINFO(&aui);
	if (hdl->sa.mode & SIO_PLAY) {
		aui.play.sample_rate = par->rate;
		aui.play.channels = par->pchan;
		sun_enctoinfo(&aui.play, par);
	}
	if (hdl->sa.mode & SIO_REC) {
		aui.record.sample_rate = par->rate;
		aui.record.channels = par->rchan;
		sun_enctoinfo(&aui.record, par);
	}
	if (ioctl(hdl->fd, AUDIO_SETINFO, &aui) < 0 && errno != EINVAL) {
		perror("sun_setpar: setinfo");
		hdl->sa.eof = 1;
		return 0;
	}

	/*
	 * if block size and buffer size are not both set then
	 * set the blocksize to half the buffer size
	 */ 
	bufsz = par->appbufsz;
	round = par->round;
	if (bufsz != (unsigned)~0) {
		if (round == (unsigned)~0)
			round = (bufsz + 1) / 2;
	} else if (round != (unsigned)~0) {
		if (bufsz == (unsigned)~0)
			bufsz = round * 2;
	} else
		return 1;

	/*
	 * get the play/record frame size in bytes
	 */
	if (ioctl(hdl->fd, AUDIO_GETINFO, &aui) < 0) {
		perror("sun_setpar: GETINFO");
		hdl->sa.eof = 1;
		return 0;
	}
	ibpf = (hdl->sa.mode & SIO_REC) ?
	    aui.record.channels * aui.record.precision / 8 : 1;
	obpf = (hdl->sa.mode & SIO_PLAY) ?
	    aui.play.channels * aui.play.precision / 8 : 1;

#ifdef DEBUG
	if (hdl->sa.debug)
		fprintf(stderr, "sun_setpar: bpf = (%u, %u)\n", ibpf, obpf);
#endif
	/*
	 * try to set parameters until the device accepts
	 * a common block size for play and record
	 */
	for (i = 0; i < NRETRIES; i++) {
		AUDIO_INITINFO(&aui);
		aui.hiwat = (bufsz + round - 1) / round;
		aui.lowat = aui.hiwat;
		if (hdl->sa.mode & SIO_REC)
			aui.record.block_size = round * ibpf;
		if (hdl->sa.mode & SIO_PLAY)
			aui.play.block_size = round * obpf;
		if (ioctl(hdl->fd, AUDIO_SETINFO, &aui) < 0) {
			perror("sun_setpar2: SETINFO");
			hdl->sa.eof = 1;
			return 0;
		}
		if (ioctl(hdl->fd, AUDIO_GETINFO, &aui) < 0) {
			perror("sun_setpar2: GETINFO");
			hdl->sa.eof = 1;
			return 0;
		}
		infr = aui.record.block_size / ibpf;
		onfr = aui.play.block_size / obpf;
#ifdef DEBUG
		if (hdl->sa.debug) {
			fprintf(stderr,
			    "sun_setpar: %i: trying rond = %u -> (%u, %u)\n",
			    i, round, infr, onfr);
		}
#endif

		/*
		 * if half-duplex or both block sizes match, we're done
		 */
		if (hdl->sa.mode != (SIO_REC | SIO_PLAY) || infr == onfr) {
#ifdef DEBUG
			if (hdl->sa.debug)
				fprintf(stderr, "sun_setpar: blocksize ok\n");
#endif
			return 1;
		}

		/*
		 * half of the retries, retry with the smaller value,
		 * then with the larger returned value
		 */
		if (i < NRETRIES / 2)
			round = infr < onfr ? infr : onfr;
		else
			round = infr < onfr ? onfr : infr;
	}
	fprintf(stderr, "sun_setpar: couldn't find a working blocksize\n");
	hdl->sa.eof = 1;
	return 0;
#undef NRETRIES
}

int
sun_getpar(struct sio_hdl *sh, struct sio_par *par)
{
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	struct audio_info aui;	

	if (ioctl(hdl->fd, AUDIO_GETINFO, &aui) < 0) {
		perror("sun_getpar: setinfo");
		hdl->sa.eof = 1;
		return 0;
	}
	if (hdl->sa.mode & SIO_PLAY) {
		par->rate = aui.play.sample_rate;
		sun_infotoenc(&aui.play, par);
	} else if (hdl->sa.mode & SIO_REC) {
		par->rate = aui.record.sample_rate;
		sun_infotoenc(&aui.record, par);
	} else
		return 0;
	par->pchan = (hdl->sa.mode & SIO_PLAY) ?
	    aui.play.channels : 0;
	par->rchan = (hdl->sa.mode & SIO_REC) ?
	    aui.record.channels : 0;
	par->round = (hdl->sa.mode & SIO_REC) ?
	    aui.record.block_size / (par->bps * par->rchan) :
	    aui.play.block_size / (par->bps * par->pchan);
	par->appbufsz = aui.hiwat * par->round;
	par->bufsz = par->appbufsz;
	return 1;
}

size_t
sun_read(struct sio_hdl *sh, void *buf, size_t len)
{
#define DROP_NMAX 0x1000
	static char dropbuf[DROP_NMAX];
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	ssize_t n, todo;

	while (hdl->offset > 0) {
		todo = hdl->offset * hdl->ibpf;
		if (todo > DROP_NMAX)
			todo = DROP_NMAX - DROP_NMAX % hdl->ibpf;
		while ((n = read(hdl->fd, dropbuf, todo)) < 0) {
			if (errno == EINTR)
				continue;
			if (errno != EAGAIN) {
				perror("sun_read: read");
				hdl->sa.eof = 1;
			}
			return 0;
		}
		if (n == 0) {
			fprintf(stderr, "sun_read: eof\n");
			hdl->sa.eof = 1;
			return 0;
		}
		hdl->offset -= (int)n / (int)hdl->ibpf;
#ifdef DEBUG
		if (hdl->sa.debug)
			fprintf(stderr, "sun_read: dropped %ld/%ld bytes "
			    "to resync\n", n, todo);
#endif
	}

	while ((n = read(hdl->fd, buf, len)) < 0) {
		if (errno == EINTR)
			continue;
		if (errno != EAGAIN) {
			perror("sun_read: read");
			hdl->sa.eof = 1;
		}
		return 0;
	}
	if (n == 0) {
		fprintf(stderr, "sun_read: eof\n");
		hdl->sa.eof = 1;
		return 0;
	}
	return n;
}

size_t
sun_autostart(struct sun_hdl *hdl)
{
	struct audio_info aui;	
	struct pollfd pfd;
	
	pfd.fd = hdl->fd;
	pfd.events = POLLOUT;
	while (poll(&pfd, 1, 0) < 0) {
		if (errno == EINTR)
			continue;
		perror("sun_fill: poll");
		hdl->sa.eof = 1;
		return 0;
	}
	if (!(pfd.revents & POLLOUT)) {
		hdl->filling = 0;
		AUDIO_INITINFO(&aui);
		if (hdl->sa.mode & SIO_PLAY)
			aui.play.pause = 0;
		if (hdl->sa.mode & SIO_REC)
			aui.record.pause = 0;
		if (ioctl(hdl->fd, AUDIO_SETINFO, &aui) < 0) {
			perror("sun_start: setinfo");
			hdl->sa.eof = 1;
			return 0;
		}
		sio_onmove_cb(&hdl->sa, 0);
	}
	return 1;
}

size_t
sun_write(struct sio_hdl *sh, void *buf, size_t len)
{
#define ZERO_NMAX 0x1000
	static char zero[ZERO_NMAX];
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	unsigned char *data = buf;
	ssize_t n, todo;

	while (hdl->offset < 0) {
		todo = (int)-hdl->offset * (int)hdl->obpf;
		if (todo > ZERO_NMAX)
			todo = ZERO_NMAX - ZERO_NMAX % hdl->obpf;
		while ((n = write(hdl->fd, zero, todo)) < 0) {
			if (errno == EINTR)
				continue;
			if (errno != EAGAIN) {
				perror("sun_write: sil");
				hdl->sa.eof = 1;
				return 0;
			}
			return 0;
		}
		hdl->offset += (int)n / (int)hdl->obpf;
#ifdef DEBUG
		if (hdl->sa.debug)
			fprintf(stderr, "sun_write: inserted %ld/%ld bytes "
			    "of silence to resync\n", n, todo);
#endif
	}

	todo = len;
	while ((n = write(hdl->fd, data, todo)) < 0) {
		if (errno == EINTR)
			continue;
		if (errno != EAGAIN) {
			perror("sun_write: write");
			hdl->sa.eof = 1;
			return 0;
		}
 		return 0;
	}
	if (hdl->filling) {
		if (!sun_autostart(hdl))
			return 0;
	}
	return n;
}

int
sun_pollfd(struct sio_hdl *sh, struct pollfd *pfd, int events)
{
	struct sun_hdl *hdl = (struct sun_hdl *)sh;

	pfd->fd = hdl->fd;
	pfd->events = events;	
	return 1;
}

int
sun_revents(struct sio_hdl *sh, struct pollfd *pfd)
{
	struct sun_hdl *hdl = (struct sun_hdl *)sh;
	struct audio_offset ao;
	int xrun, dmove, dierr = 0, doerr = 0, doffset = 0;
	int revents = pfd->revents;

	if (hdl->sa.mode & SIO_PLAY) {
		if (ioctl(hdl->fd, AUDIO_PERROR, &xrun) < 0) {
			perror("sun_revents: PERROR");
			exit(1);
		}
		doerr = xrun - hdl->oerr;
		hdl->oerr = xrun;
		if (hdl->sa.mode & SIO_REC)
			doffset += doerr;
	}
	if (hdl->sa.mode & SIO_REC) {
		if (ioctl(hdl->fd, AUDIO_RERROR, &xrun) < 0) {
			perror("sun_revents: RERROR");
			exit(1);
		}
		dierr = xrun - hdl->ierr;
		hdl->ierr = xrun;
		if (hdl->sa.mode & SIO_PLAY)
			doffset -= dierr;
	}
	hdl->offset += doffset;
	dmove = dierr > doerr ? dierr : doerr;
	hdl->idelta -= dmove;
	hdl->odelta -= dmove;

	if ((revents & POLLOUT) && !(hdl->sa.mode & SIO_REC)) {
		if (ioctl(hdl->fd, AUDIO_GETOOFFS, &ao) < 0) {
			perror("sun_revents: GETOOFFS");
			exit(1);
		}
		hdl->odelta += (ao.samples - hdl->obytes) / hdl->obpf;
		hdl->obytes = ao.samples;
		if (hdl->odelta != 0) {
			sio_onmove_cb(&hdl->sa, hdl->odelta);
			hdl->odelta = 0;
		}
	}
	if ((revents & POLLIN) && (hdl->sa.mode & SIO_REC)) {
		if (ioctl(hdl->fd, AUDIO_GETIOFFS, &ao) < 0) {
			perror("sun_revents: GETIOFFS");
			exit(1);
		}
		hdl->idelta += (ao.samples - hdl->ibytes) / hdl->ibpf;
		hdl->ibytes = ao.samples;
		if (hdl->idelta != 0) {
			sio_onmove_cb(&hdl->sa, hdl->idelta);
			hdl->idelta = 0;
		}
	}
	if (hdl->filling)
		revents |= POLLOUT;
	return revents;
}

