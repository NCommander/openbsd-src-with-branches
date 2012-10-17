/*	$OpenBSD: mio.c,v 1.14 2012/04/11 06:05:43 ratchov Exp $	*/
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "mio_priv.h"

struct mio_hdl *
mio_open(const char *str, unsigned int mode, int nbio)
{
	static char portany[] = MIO_PORTANY;
	struct mio_hdl *hdl;
	const char *p;

#ifdef DEBUG
	sndio_debug_init();
#endif
	if ((mode & (MIO_OUT | MIO_IN)) == 0)
		return NULL;
	if (str == NULL) /* backward compat */
		str = portany;
	if (strcmp(str, portany) == 0 && !issetugid()) {
		str = getenv("MIDIDEVICE");
		if (str == NULL)
			str = portany;
	}
	if (strcmp(str, portany) == 0) {
		hdl = mio_aucat_open("/0", mode, nbio, 1);
		if (hdl != NULL)
			return hdl;
		return mio_rmidi_open("/0", mode, nbio);
	}
	if ((p = sndio_parsetype(str, "snd")) != NULL ||
	    (p = sndio_parsetype(str, "aucat")) != NULL)
		return mio_aucat_open(p, mode, nbio, 0);
	if ((p = sndio_parsetype(str, "midithru")) != NULL)
		return mio_aucat_open(p, mode, nbio, 1);
	if ((p = sndio_parsetype(str, "rmidi")) != NULL)
		return mio_rmidi_open(p, mode, nbio);
	DPRINTF("mio_open: %s: unknown device type\n", str);
	return NULL;
}

void
mio_create(struct mio_hdl *hdl, struct mio_ops *ops,
    unsigned int mode, int nbio)
{
	hdl->ops = ops;
	hdl->mode = mode;
	hdl->nbio = nbio;
	hdl->eof = 0;
}

void
mio_close(struct mio_hdl *hdl)
{
	hdl->ops->close(hdl);
}

size_t
mio_read(struct mio_hdl *hdl, void *buf, size_t len)
{
	if (hdl->eof) {
		DPRINTF("mio_read: eof\n");
		return 0;
	}
	if (!(hdl->mode & MIO_IN)) {
		DPRINTF("mio_read: not input device\n");
		hdl->eof = 1;
		return 0;
	}
	if (len == 0) {
		DPRINTF("mio_read: zero length read ignored\n");
		return 0;
	}
	return hdl->ops->read(hdl, buf, len);
}

size_t
mio_write(struct mio_hdl *hdl, const void *buf, size_t len)
{
	if (hdl->eof) {
		DPRINTF("mio_write: eof\n");
		return 0;
	}
	if (!(hdl->mode & MIO_OUT)) {
		DPRINTF("mio_write: not output device\n");
		hdl->eof = 1;
		return 0;
	}
	if (len == 0) {
		DPRINTF("mio_write: zero length write ignored\n");
		return 0;
	}
	return hdl->ops->write(hdl, buf, len);
}

int
mio_nfds(struct mio_hdl *hdl)
{
	return 1;
}

int
mio_pollfd(struct mio_hdl *hdl, struct pollfd *pfd, int events)
{
	if (hdl->eof)
		return 0;
	return hdl->ops->pollfd(hdl, pfd, events);
}

int
mio_revents(struct mio_hdl *hdl, struct pollfd *pfd)
{
	if (hdl->eof)
		return POLLHUP;
	return hdl->ops->revents(hdl, pfd);
}

int
mio_eof(struct mio_hdl *hdl)
{
	return hdl->eof;
}
