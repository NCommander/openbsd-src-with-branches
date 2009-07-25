/*	$OpenBSD: sndio.c,v 1.16 2009/07/25 08:44:27 ratchov Exp $	*/
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
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "sndio_priv.h"

#define SIO_PAR_MAGIC	0x83b905a4

#ifdef DEBUG
/*
 * debug level, -1 means uninitialized
 */
int sio_debug = -1;
#endif

void
sio_initpar(struct sio_par *par)
{
	memset(par, 0xff, sizeof(struct sio_par));
	par->__magic = SIO_PAR_MAGIC;	   
}

/*
 * Generate a string corresponding to the encoding in par,
 * return the length of the resulting string
 */
int
sio_enctostr(struct sio_par *par, char *ostr)
{
	char *p = ostr;

	*p++ = par->sig ? 's' : 'u';
	if (par->bits > 9)
		*p++ = '0' + par->bits / 10;
	*p++ = '0' + par->bits % 10;
	if (par->bps > 1) {
		*p++ = par->le ? 'l' : 'b';
		*p++ = 'e';
		if (par->bps != SIO_BPS(par->bits) ||
		    par->bits < par->bps * 8) {
			*p++ = par->bps + '0';
			if (par->bits < par->bps * 8) {
				*p++ = par->msb ? 'm' : 'l';
				*p++ = 's';
				*p++ = 'b';
			}
		}
	}
	*p++ = '\0';
	return p - ostr - 1;
}

/*
 * Parse an encoding string, examples: s8, u8, s16, s16le, s24be ...
 * Return the number of bytes consumed
 */
int
sio_strtoenc(struct sio_par *par, char *istr)
{
	char *p = istr;
	int i, sig, bits, le, bps, msb;
	
#define IS_SEP(c)			\
	(((c) < 'a' || (c) > 'z') &&	\
	 ((c) < 'A' || (c) > 'Z') &&	\
	 ((c) < '0' || (c) > '9'))

	/*
	 * get signedness
	 */
	if (*p == 's') {
		sig = 1;
	} else if (*p == 'u') {
		sig = 0;
	} else
		return 0;
	p++;
	
	/*
	 * get number of bits per sample
	 */
	bits = 0;
	for (i = 0; i < 2; i++) {
		if (*p < '0' || *p > '9')
			break;
		bits = (bits * 10) + *p - '0';
		p++;
	}
	if (bits < 1 || bits > 32)
		return 0;
	bps = SIO_BPS(bits);
	le = SIO_LE_NATIVE;
	msb = 1;

	/*
	 * get (optional) endianness
	 */
	if (p[0] == 'l' && p[1] == 'e') {
		le = 1;
		p += 2;
	} else if (p[0] == 'b' && p[1] == 'e') {
		le = 0;
		p += 2;
	} else if (IS_SEP(*p)) {
		goto done;
	} else
		return 0;

	/*
	 * get (optional) number of bytes
	 */
	if (*p >= '1' && *p <= '4') {
		bps = *p - '0';
		if (bps * 8  < bits)
			return 0;
		p++;

		/*
		 * get (optional) alignment
		 */
		if (p[0] == 'm' && p[1] == 's' && p[2] == 'b') {
			msb = 1;
			p += 3;
		} else if (p[0] == 'l' && p[1] == 's' && p[2] == 'b') {
			msb = 0;
			p += 3;
		} else if (IS_SEP(*p)) {
			goto done;
		} else
			return 0;
	} else if (!IS_SEP(*p))
		return 0;

done:
       	par->msb = msb;
	par->sig = sig;
	par->bits = bits;
	par->bps = bps;
	par->le = le;
	return p - istr;
}

struct sio_hdl *
sio_open(const char *str, unsigned mode, int nbio)
{
	static char prefix_aucat[] = "aucat";
	static char prefix_sun[] = "sun";
	struct sio_hdl *hdl;
	struct stat sb;
	char *sep, buf[NAME_MAX];
	int len;
#ifdef DEBUG
	char *dbg;

	if (sio_debug < 0) {
		dbg = issetugid() ? NULL : getenv("SIO_DEBUG");
		if (!dbg || sscanf(dbg, "%u", &sio_debug) != 1)
			sio_debug = 0;
	}
#endif	
	if ((mode & (SIO_PLAY | SIO_REC)) == 0)
		return NULL;
	if (str == NULL && !issetugid())
		str = getenv("AUDIODEVICE");
	if (str == NULL) {
		hdl = sio_open_aucat("0", mode, nbio);
		if (hdl != NULL)
			return hdl;
		if (stat("/dev/audio", &sb) == 0 && S_ISCHR(sb.st_mode)) {
			snprintf(buf, sizeof(buf), "%u",
			    minor(sb.st_rdev) & 0xf);
		} else
			strlcpy(buf, "0", sizeof(buf));
		return sio_open_sun(buf, mode, nbio);
	}
	sep = strchr(str, ':');
	if (sep == NULL) {
		/*
		 * try legacy "/dev/audioxxx" or ``socket'' device name
		 */
		if (stat(str, &sb) < 0 || !S_ISCHR(sb.st_mode)) {
			snprintf(buf, sizeof(buf), "0.%s", str);
			return sio_open_aucat(buf, mode, nbio);
		}
		snprintf(buf, sizeof(buf), "%u", minor(sb.st_rdev) & 0xf);
		return sio_open_sun(buf, mode, nbio);
	}
	len = sep - str;
	if (len == strlen(prefix_aucat) &&
	    memcmp(str, prefix_aucat, len) == 0)
		return sio_open_aucat(sep + 1, mode, nbio);
	if (len == strlen(prefix_sun) &&
	    memcmp(str, prefix_sun, len) == 0)
		return sio_open_sun(sep + 1, mode, nbio);
	DPRINTF("sio_open: %s: unknown device type\n", str);
	return NULL;
}

void
sio_create(struct sio_hdl *hdl, struct sio_ops *ops, unsigned mode, int nbio)
{
	hdl->ops = ops;
	hdl->mode = mode;
	hdl->nbio = nbio;
	hdl->started = 0;
	hdl->eof = 0;
	hdl->move_cb = NULL;
	hdl->vol_cb = NULL;
}

void
sio_close(struct sio_hdl *hdl)
{
	return hdl->ops->close(hdl);
}

int
sio_start(struct sio_hdl *hdl)
{
	if (hdl->eof) {
		DPRINTF("sio_start: eof\n");
		return 0;
	}
	if (hdl->started) {
		DPRINTF("sio_start: already started\n");
		hdl->eof = 1;
		return 0;
	}
#ifdef DEBUG
	if (!sio_getpar(hdl, &hdl->par))
		return 0;
	hdl->pollcnt = hdl->wcnt = hdl->rcnt = hdl->realpos = 0;
	gettimeofday(&hdl->tv, NULL);
#endif
	if (!hdl->ops->start(hdl))
		return 0;
	hdl->started = 1;
	return 1;
}

int
sio_stop(struct sio_hdl *hdl)
{
	if (hdl->eof) {
		DPRINTF("sio_stop: eof\n");
		return 0;
	}
	if (!hdl->started) {
		DPRINTF("sio_stop: not started\n");
		hdl->eof = 1;
		return 0;
	}
	if (!hdl->ops->stop(hdl))
		return 0;
#ifdef DEBUG
	DPRINTF("libsndio: polls: %llu, written = %llu, read: %llu\n",
	    hdl->pollcnt, hdl->wcnt, hdl->rcnt);
#endif
	hdl->started = 0;
	return 1;
}

int
sio_setpar(struct sio_hdl *hdl, struct sio_par *par)
{
	if (hdl->eof) {
		DPRINTF("sio_setpar: eof\n");
		return 0;
	}
	if (par->__magic != SIO_PAR_MAGIC) {
		DPRINTF("sio_setpar: use of uninitialized sio_par structure\n");
		hdl->eof = 1;
		return 0;
	}
	if (hdl->started) {
		DPRINTF("sio_setpar: already started\n");
		hdl->eof = 1;
		return 0;
	}
	if (par->bufsz != (unsigned)~0) {
		DPRINTF("sio_setpar: setting bufsz is deprecated\n");
		par->appbufsz = par->bufsz;
	}
	if (par->rate != (unsigned)~0 && par->appbufsz == (unsigned)~0)
		par->appbufsz = par->rate * 200 / 1000;
	return hdl->ops->setpar(hdl, par);
}

int
sio_getpar(struct sio_hdl *hdl, struct sio_par *par)
{
	if (hdl->eof) {
		DPRINTF("sio_getpar: eof\n");
		return 0;
	}
	if (hdl->started) {
		DPRINTF("sio_getpar: already started\n");
		hdl->eof = 1;
		return 0;
	}
	if (!hdl->ops->getpar(hdl, par)) {
		par->__magic = 0;
		return 0;
	}
	par->__magic = 0;
	return 1;
}

int
sio_getcap(struct sio_hdl *hdl, struct sio_cap *cap)
{
	if (hdl->eof) {
		DPRINTF("sio_getcap: eof\n");
		return 0;
	}
	if (hdl->started) {
		DPRINTF("sio_getcap: already started\n");
		hdl->eof = 1;
		return 0;
	}
	return hdl->ops->getcap(hdl, cap);
}

static int
sio_psleep(struct sio_hdl *hdl, int event)
{
	struct pollfd pfd;
	int revents;

	for (;;) {
		sio_pollfd(hdl, &pfd, event);
		while (poll(&pfd, 1, -1) < 0) {
			if (errno == EINTR)
				continue;
			DPERROR("sio_psleep: poll");
			hdl->eof = 1;
			return 0;
		}
		revents = sio_revents(hdl, &pfd);
		if (revents & POLLHUP) {
			DPRINTF("sio_psleep: hang-up\n");
			return 0;
		}
		if (revents & event)
			break;
	}
	return 1;
}

size_t
sio_read(struct sio_hdl *hdl, void *buf, size_t len)
{
	unsigned n;
	char *data = buf;
	size_t todo = len;

	if (hdl->eof) {
		DPRINTF("sio_read: eof\n");
		return 0;
	}
	if (!hdl->started || !(hdl->mode & SIO_REC)) {
		DPRINTF("sio_read: recording not started\n");
		hdl->eof = 1;
		return 0;
	}
	if (todo == 0) {
		DPRINTF("sio_read: zero length read ignored\n");
		return 0;
	}
	while (todo > 0) {
		n = hdl->ops->read(hdl, data, todo);
		if (n == 0) {
			if (hdl->nbio || hdl->eof || todo < len)
				break;
			if (!sio_psleep(hdl, POLLIN))
				break;
			continue;
		}
		data += n;
		todo -= n;
#ifdef DEBUG
		hdl->rcnt += n;
#endif
	}
	return len - todo;
}

size_t
sio_write(struct sio_hdl *hdl, const void *buf, size_t len)
{
	unsigned n;
	const unsigned char *data = buf;
	size_t todo = len;
#ifdef DEBUG
	struct timeval tv0, tv1, dtv;
	unsigned us;

	if (sio_debug >= 2)
		gettimeofday(&tv0, NULL);
#endif

	if (hdl->eof) {
		DPRINTF("sio_write: eof\n");
		return 0;
	}
	if (!hdl->started || !(hdl->mode & SIO_PLAY)) {
		DPRINTF("sio_write: playback not started\n");
		hdl->eof = 1;
		return 0;
	}
	if (todo == 0) {
		DPRINTF("sio_write: zero length write ignored\n");
		return 0;
	}
	while (todo > 0) {
		n = hdl->ops->write(hdl, data, todo);
		if (n == 0) {
			if (hdl->nbio || hdl->eof)
				break;
			if (!sio_psleep(hdl, POLLOUT))
				break;
			continue;
		}
		data += n;
		todo -= n;
#ifdef DEBUG
		hdl->wcnt += n;
#endif
	}
#ifdef DEBUG
	if (sio_debug >= 2) {
		gettimeofday(&tv1, NULL);
		timersub(&tv0, &hdl->tv, &dtv);
		DPRINTF("%ld.%06ld: ", dtv.tv_sec, dtv.tv_usec);

		timersub(&tv1, &tv0, &dtv);
		us = dtv.tv_sec * 1000000 + dtv.tv_usec; 
		DPRINTF(
		    "sio_write: wrote %d bytes of %d in %uus\n",
		    (int)(len - todo), (int)len, us);
	}
#endif
	return len - todo;
}

int
sio_nfds(struct sio_hdl *hdl)
{
	/*
	 * In the future we might use larger values
	 */
	return 1;
}

int
sio_pollfd(struct sio_hdl *hdl, struct pollfd *pfd, int events)
{
	if (hdl->eof)
		return 0;
	if (!hdl->started)
		events = 0;
	return hdl->ops->pollfd(hdl, pfd, events);
}

int
sio_revents(struct sio_hdl *hdl, struct pollfd *pfd)
{	
	int revents;
#ifdef DEBUG
	struct timeval tv0, tv1, dtv;
	unsigned us;

	if (sio_debug >= 2)
		gettimeofday(&tv0, NULL);
#endif
	if (hdl->eof)
		return POLLHUP;
#ifdef DEBUG
	hdl->pollcnt++;
#endif
	revents = hdl->ops->revents(hdl, pfd);
	if (!hdl->started)
		return revents & POLLHUP;
#ifdef DEBUG
	if (sio_debug >= 2) {
		gettimeofday(&tv1, NULL);
		timersub(&tv0, &hdl->tv, &dtv);
		DPRINTF("%ld.%06ld: ", dtv.tv_sec, dtv.tv_usec);

		timersub(&tv1, &tv0, &dtv);
		us = dtv.tv_sec * 1000000 + dtv.tv_usec; 
		DPRINTF("sio_revents: revents = 0x%x, complete in %uus\n",
		    revents, us);
	}
#endif
	return revents;
}

int
sio_eof(struct sio_hdl *hdl)
{
	return hdl->eof;
}

void
sio_onmove(struct sio_hdl *hdl, void (*cb)(void *, int), void *addr)
{
	if (hdl->started) {
		DPRINTF("sio_onmove: already started\n");
		hdl->eof = 1;
		return;
	}
	hdl->move_cb = cb;
	hdl->move_addr = addr;
}

void
sio_onmove_cb(struct sio_hdl *hdl, int delta)
{
#ifdef DEBUG
	struct timeval tv0, dtv;
	long long playpos;

	if (sio_debug >= 2 && (hdl->mode & SIO_PLAY)) {
		gettimeofday(&tv0, NULL);
		timersub(&tv0, &hdl->tv, &dtv);
		DPRINTF("%ld.%06ld: ", dtv.tv_sec, dtv.tv_usec);
		hdl->realpos += delta;
		playpos = hdl->wcnt / (hdl->par.bps * hdl->par.pchan);
		DPRINTF("sio_onmove_cb: delta = %+7d, "
		    "plat = %+7lld, "
		    "realpos = %+7lld, "
		    "bufused = %+7lld\n",
		    delta,
		    playpos - hdl->realpos,
		    hdl->realpos,
		    hdl->realpos < 0 ? playpos : playpos - hdl->realpos);
	}
#endif
	if (hdl->move_cb)
		hdl->move_cb(hdl->move_addr, delta);
}

int
sio_setvol(struct sio_hdl *hdl, unsigned ctl)
{
	if (hdl->eof)
		return 0;
	if (!hdl->ops->setvol(hdl, ctl))
		return 0;
	hdl->ops->getvol(hdl);
	return 1;
}

void
sio_onvol(struct sio_hdl *hdl, void (*cb)(void *, unsigned), void *addr)
{
	if (hdl->started) {
		DPRINTF("sio_onvol: already started\n");
		hdl->eof = 1;
		return;
	}
	hdl->vol_cb = cb;
	hdl->vol_addr = addr;
	hdl->ops->getvol(hdl);
}

void
sio_onvol_cb(struct sio_hdl *hdl, unsigned ctl)
{
	if (hdl->vol_cb)
		hdl->vol_cb(hdl->vol_addr, ctl);	
}
