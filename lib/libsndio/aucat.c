/*	$OpenBSD: aucat.c,v 1.16 2009/05/15 13:04:52 ratchov Exp $	*/
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
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "amsg.h"
#include "sndio_priv.h"

struct aucat_hdl {
	struct sio_hdl sio;
	int fd;				/* socket */
	struct amsg rmsg, wmsg;		/* temporary messages */
	size_t wtodo, rtodo;		/* bytes to complete the packet */
#define STATE_IDLE	0		/* nothing to do */
#define STATE_MSG	1		/* message being transferred */
#define STATE_DATA	2		/* data being transferred */
	unsigned rstate, wstate;	/* one of above */
	unsigned rbpf, wbpf;		/* read and write bytes-per-frame */
	int maxwrite;			/* latency constraint */
	int events;			/* events the user requested */
	unsigned curvol, reqvol;	/* current and requested volume */
};

static void aucat_close(struct sio_hdl *);
static int aucat_start(struct sio_hdl *);
static int aucat_stop(struct sio_hdl *);
static int aucat_setpar(struct sio_hdl *, struct sio_par *);
static int aucat_getpar(struct sio_hdl *, struct sio_par *);
static int aucat_getcap(struct sio_hdl *, struct sio_cap *);
static size_t aucat_read(struct sio_hdl *, void *, size_t);
static size_t aucat_write(struct sio_hdl *, void *, size_t);
static int aucat_pollfd(struct sio_hdl *, struct pollfd *, int);
static int aucat_revents(struct sio_hdl *, struct pollfd *);
static int aucat_setvol(struct sio_hdl *, unsigned);
static void aucat_getvol(struct sio_hdl *);

static struct sio_ops aucat_ops = {
	aucat_close,
	aucat_setpar,
	aucat_getpar,
	aucat_getcap,
	aucat_write,
	aucat_read,
	aucat_start,
	aucat_stop,
	aucat_pollfd,
	aucat_revents,
	aucat_setvol,
	aucat_getvol
};

struct sio_hdl *
sio_open_aucat(char *path, unsigned mode, int nbio)
{
	int s;
	struct sio_cap cap;
	struct aucat_hdl *hdl;
	struct sockaddr_un ca;	
	socklen_t len = sizeof(struct sockaddr_un);
	uid_t uid;

	if (path == NULL)
		path = SIO_AUCAT_PATH;
	uid = geteuid();
	if (strchr(path, '/') != NULL)
		return NULL;
	snprintf(ca.sun_path, sizeof(ca.sun_path),
	    "/tmp/aucat-%u/%s", uid, path);
	ca.sun_family = AF_UNIX;

	hdl = malloc(sizeof(struct aucat_hdl));
	if (hdl == NULL)
		return NULL;
	sio_create(&hdl->sio, &aucat_ops, mode, nbio);	

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0)
		goto bad_free;
	while (connect(s, (struct sockaddr *)&ca, len) < 0) {
		if (errno == EINTR)
			continue;
		goto bad_connect;
	}
	if (fcntl(s, F_SETFD, FD_CLOEXEC) < 0) {
		DPERROR("FD_CLOEXEC");
		goto bad_connect;
	}
	hdl->fd = s;
	hdl->rstate = STATE_IDLE;
	hdl->rtodo = 0xdeadbeef;
	hdl->wstate = STATE_IDLE;
	hdl->wtodo = 0xdeadbeef;
	hdl->curvol = SIO_MAXVOL;
	hdl->reqvol = SIO_MAXVOL;
	if (!sio_getcap(&hdl->sio, &cap))
		goto bad_connect;
	if (((mode & SIO_PLAY) && cap.confs[0].pchan == 0) ||
	    ((mode & SIO_REC)  && cap.confs[0].rchan == 0))
		goto bad_connect;
	return (struct sio_hdl *)hdl;
 bad_connect:
	while (close(s) < 0 && errno == EINTR)
		; /* retry */
 bad_free:
	free(hdl);
	return NULL;
}

/*
 * read a message, return 0 if blocked
 */
static int
aucat_rmsg(struct aucat_hdl *hdl)
{
	ssize_t n;
	unsigned char *data;

	while (hdl->rtodo > 0) {
		data = (unsigned char *)&hdl->rmsg;
		data += sizeof(struct amsg) - hdl->rtodo;
		while ((n = read(hdl->fd, data, hdl->rtodo)) < 0) {
			if (errno == EINTR)
				continue;
			if (errno != EAGAIN) {
				hdl->sio.eof = 1;
				DPERROR("aucat_rmsg: read");
			}
			return 0;
		}
		if (n == 0) {
			DPRINTF("aucat_rmsg: eof\n");
			hdl->sio.eof = 1;
			return 0;
		}
		hdl->rtodo -= n;
	}
	return 1;
}

/*
 * write a message, return 0 if blocked
 */
static int
aucat_wmsg(struct aucat_hdl *hdl)
{
	ssize_t n;
	unsigned char *data;

	while (hdl->wtodo > 0) {
		data = (unsigned char *)&hdl->wmsg;
		data += sizeof(struct amsg) - hdl->wtodo;
		while ((n = write(hdl->fd, data, hdl->wtodo)) < 0) {
			if (errno == EINTR)
				continue;
			if (errno != EAGAIN) {
				hdl->sio.eof = 1;
				DPERROR("aucat_wmsg: write");
			}
			return 0;
		}
		hdl->wtodo -= n;
	}
	return 1;
}

/*
 * execute the next message, return 0 if blocked
 */
static int
aucat_runmsg(struct aucat_hdl *hdl)
{
	if (!aucat_rmsg(hdl))
		return 0;
	switch (hdl->rmsg.cmd) {
	case AMSG_DATA:
		if (hdl->rmsg.u.data.size == 0 ||
		    hdl->rmsg.u.data.size % hdl->rbpf) {
			DPRINTF("aucat_runmsg: bad data message\n");
			hdl->sio.eof = 1;
			return 0;
		}
		hdl->rstate = STATE_DATA;
		hdl->rtodo = hdl->rmsg.u.data.size;
		break;
	case AMSG_MOVE:
		hdl->maxwrite += hdl->rmsg.u.ts.delta * (int)hdl->wbpf;
		sio_onmove_cb(&hdl->sio, hdl->rmsg.u.ts.delta);
		hdl->rstate = STATE_MSG;
		hdl->rtodo = sizeof(struct amsg);
		break;
	case AMSG_GETPAR:
	case AMSG_ACK:
		hdl->rstate = STATE_IDLE;
		hdl->rtodo = 0xdeadbeef;
		break;
	default:
		DPRINTF("aucat_runmsg: unknown message\n");
		hdl->sio.eof = 1;
		return 0;
	}
	return 1;
}

static void
aucat_close(struct sio_hdl *sh)
{
	struct aucat_hdl *hdl = (struct aucat_hdl *)sh;

	while (close(hdl->fd) < 0 && errno == EINTR)
		; /* nothing */
	free(hdl);
}

static int
aucat_start(struct sio_hdl *sh)
{
	struct aucat_hdl *hdl = (struct aucat_hdl *)sh;
	struct sio_par par;

	/*
	 * save bpf
	 */
	if (!sio_getpar(&hdl->sio, &par))
		return 0;
	hdl->wbpf = par.bps * par.pchan;
	hdl->rbpf = par.bps * par.rchan;
	hdl->maxwrite = hdl->wbpf * par.bufsz;

	AMSG_INIT(&hdl->wmsg);
	hdl->wmsg.cmd = AMSG_START;
	hdl->wtodo = sizeof(struct amsg);
	if (!aucat_wmsg(hdl))
		return 0;
	hdl->rstate = STATE_MSG;
	hdl->rtodo = sizeof(struct amsg);
	if (fcntl(hdl->fd, F_SETFL, O_NONBLOCK) < 0) {
		DPERROR("aucat_start: fcntl(0)");
		hdl->sio.eof = 1;
		return 0;
	}
	return 1;
}

static int
aucat_stop(struct sio_hdl *sh)
{
#define ZERO_MAX 0x400
	static unsigned char zero[ZERO_MAX];
	struct aucat_hdl *hdl = (struct aucat_hdl *)sh;
	unsigned n, count, todo;

	if (fcntl(hdl->fd, F_SETFL, 0) < 0) {
		DPERROR("aucat_stop: fcntl(0)");
		hdl->sio.eof = 1;
		return 0;
	}

	/*
	 * complete data block in progress
	 */
	if (hdl->wstate != STATE_IDLE) {
		todo = (hdl->wstate == STATE_MSG) ? 
		    hdl->wmsg.u.data.size : hdl->wtodo;
		hdl->maxwrite = todo;
		memset(zero, 0, ZERO_MAX);
		while (todo > 0) {
			count = todo;
			if (count > ZERO_MAX)
				count = ZERO_MAX;
			n = aucat_write(&hdl->sio, zero, count);
			if (n == 0)
				return 0;
			todo -= n;
		}
	}

	/*
	 * send stop message
	 */
	AMSG_INIT(&hdl->wmsg);
	hdl->wmsg.cmd = AMSG_STOP;
	hdl->wtodo = sizeof(struct amsg);
	if (!aucat_wmsg(hdl))
		return 0;
	if (hdl->rstate == STATE_IDLE) {
		hdl->rstate = STATE_MSG;
		hdl->rtodo = sizeof(struct amsg);
	}

	/*
	 * wait for the STOP ACK
	 */
	while (hdl->rstate != STATE_IDLE) {
		switch (hdl->rstate) {
		case STATE_MSG:
			if (!aucat_runmsg(hdl))
				return 0;
			break;
		case STATE_DATA:
			if (!aucat_read(&hdl->sio, zero, ZERO_MAX))
				return 0;
			break;
		}
	}
	return 1;
}

static int
aucat_setpar(struct sio_hdl *sh, struct sio_par *par)
{
	struct aucat_hdl *hdl = (struct aucat_hdl *)sh;

	AMSG_INIT(&hdl->wmsg);
	hdl->wmsg.cmd = AMSG_SETPAR;
	hdl->wmsg.u.par.bits = par->bits;
	hdl->wmsg.u.par.bps = par->bps;
	hdl->wmsg.u.par.sig = par->sig;
	hdl->wmsg.u.par.le = par->le;
	hdl->wmsg.u.par.msb = par->msb;
	hdl->wmsg.u.par.rate = par->rate;
	hdl->wmsg.u.par.appbufsz = par->appbufsz;
	hdl->wmsg.u.par.xrun = par->xrun;
	hdl->wmsg.u.par.mode = hdl->sio.mode;
	if (hdl->sio.mode & SIO_REC)
		hdl->wmsg.u.par.rchan = par->rchan;
	if (hdl->sio.mode & SIO_PLAY)
		hdl->wmsg.u.par.pchan = par->pchan;
	hdl->wtodo = sizeof(struct amsg);
	if (!aucat_wmsg(hdl))
		return 0;
	return 1;
}

static int
aucat_getpar(struct sio_hdl *sh, struct sio_par *par)
{
	struct aucat_hdl *hdl = (struct aucat_hdl *)sh;

	AMSG_INIT(&hdl->rmsg);
	hdl->wmsg.cmd = AMSG_GETPAR;
	hdl->wtodo = sizeof(struct amsg);
	if (!aucat_wmsg(hdl))
		return 0;
	hdl->rtodo = sizeof(struct amsg);
	if (!aucat_rmsg(hdl))
		return 0;
	if (hdl->rmsg.cmd != AMSG_GETPAR) {
		DPRINTF("aucat_getpar: protocol err\n");
		hdl->sio.eof = 1;
		return 0;
	}
	par->bits = hdl->rmsg.u.par.bits;
	par->bps = hdl->rmsg.u.par.bps;
	par->sig = hdl->rmsg.u.par.sig;
	par->le = hdl->rmsg.u.par.le;
	par->msb = hdl->rmsg.u.par.msb;
	par->rate = hdl->rmsg.u.par.rate;
	par->bufsz = hdl->rmsg.u.par.bufsz;
	par->appbufsz = hdl->rmsg.u.par.appbufsz;
	par->xrun = hdl->rmsg.u.par.xrun;
	par->round = hdl->rmsg.u.par.round;
	if (hdl->sio.mode & SIO_PLAY)
		par->pchan = hdl->rmsg.u.par.pchan;
	if (hdl->sio.mode & SIO_REC)
		par->rchan = hdl->rmsg.u.par.rchan;
	return 1;
}

static int
aucat_getcap(struct sio_hdl *sh, struct sio_cap *cap)
{
	struct aucat_hdl *hdl = (struct aucat_hdl *)sh;

	AMSG_INIT(&hdl->rmsg);
	hdl->wmsg.cmd = AMSG_GETCAP;
	hdl->wtodo = sizeof(struct amsg);
	if (!aucat_wmsg(hdl))
		return 0;
	hdl->rtodo = sizeof(struct amsg);
	if (!aucat_rmsg(hdl))
		return 0;
	if (hdl->rmsg.cmd != AMSG_GETCAP) {
		DPRINTF("aucat_getcap: protocol err\n");
		hdl->sio.eof = 1;
		return 0;
	}
	cap->enc[0].bits = hdl->rmsg.u.cap.bits;
	cap->enc[0].bps = SIO_BPS(hdl->rmsg.u.cap.bits);
	cap->enc[0].sig = 1;
	cap->enc[0].le = SIO_LE_NATIVE;
	cap->enc[0].msb = 1;
	cap->rchan[0] = hdl->rmsg.u.cap.rchan;
	cap->pchan[0] = hdl->rmsg.u.cap.pchan;
	cap->rate[0] = hdl->rmsg.u.cap.rate;
	cap->confs[0].enc = 1;
	cap->confs[0].rchan = (hdl->rmsg.u.cap.rchan > 0) ? 1 : 0;
	cap->confs[0].pchan = (hdl->rmsg.u.cap.pchan > 0) ? 1 : 0;
	cap->confs[0].rate = 1;
	cap->nconf = 1;
	return 1;
}

static size_t
aucat_read(struct sio_hdl *sh, void *buf, size_t len)
{
	struct aucat_hdl *hdl = (struct aucat_hdl *)sh;
	ssize_t n;

	while (hdl->rstate != STATE_DATA) {
		switch (hdl->rstate) {
		case STATE_MSG:
			if (!aucat_runmsg(hdl))
				return 0;
			break;
		case STATE_IDLE:
			DPRINTF("aucat_read: unexpected idle\n");
			break;
		}
	}
	if (len > hdl->rtodo)
		len = hdl->rtodo;
	while ((n = read(hdl->fd, buf, len)) < 0) {
		if (errno == EINTR)
			continue;
		if (errno != EAGAIN) {
			hdl->sio.eof = 1;
			DPERROR("aucat_read: read");
		}
		return 0;
	}
	if (n == 0) {
		DPRINTF("aucat_read: eof\n");
		hdl->sio.eof = 1;
		return 0;
	}
	hdl->rtodo -= n;
	if (hdl->rtodo == 0) {
		hdl->rstate = STATE_MSG;
		hdl->rtodo = sizeof(struct amsg);
	}
	return n;
}

static int
aucat_buildmsg(struct aucat_hdl *hdl, size_t len)
{
	unsigned sz;

	if (hdl->curvol != hdl->reqvol) {
		hdl->wstate = STATE_MSG;
		hdl->wtodo = sizeof(struct amsg);
		hdl->wmsg.cmd = AMSG_SETVOL;
		hdl->wmsg.u.vol.ctl = hdl->reqvol;
		hdl->curvol = hdl->reqvol;
		return 1;
	} else if (len > 0) {
		sz = (len < AMSG_DATAMAX) ? len : AMSG_DATAMAX;
		sz -= sz % hdl->wbpf;
		if (sz == 0)
			sz = hdl->wbpf;
		hdl->wstate = STATE_MSG;
		hdl->wtodo = sizeof(struct amsg);
		hdl->wmsg.cmd = AMSG_DATA;
		hdl->wmsg.u.data.size = sz;
		return 1;
	}
	return 0;
}

static size_t
aucat_write(struct sio_hdl *sh, void *buf, size_t len)
{
	struct aucat_hdl *hdl = (struct aucat_hdl *)sh;
	ssize_t n;

	while (hdl->wstate != STATE_DATA) {
		switch (hdl->wstate) {
		case STATE_IDLE:
			if (!aucat_buildmsg(hdl, len))
				return 0;
			/* PASSTHROUGH */
		case STATE_MSG:
			if (!aucat_wmsg(hdl))
				return 0;
			if (hdl->wmsg.cmd == AMSG_DATA) {
				hdl->wstate = STATE_DATA;
				hdl->wtodo = hdl->wmsg.u.data.size;
			} else
				hdl->wstate = STATE_IDLE;
			break;
		default:
			DPRINTF("aucat_write: bad state\n");
			abort();
		}
	}
	if (hdl->maxwrite <= 0)
		return 0;
	if (len > hdl->maxwrite)
		len = hdl->maxwrite;
	if (len > hdl->wtodo)
		len = hdl->wtodo;
	if (len == 0) {
		DPRINTF("aucat_write: len == 0\n");
		abort();
	}
	while ((n = write(hdl->fd, buf, len)) < 0) {
		if (errno == EINTR)
			continue;
		if (errno != EAGAIN) {
			hdl->sio.eof = 1;
			DPERROR("aucat_write: write");
		}
		return 0;
	}
	hdl->maxwrite -= n;
	hdl->wtodo -= n;
	if (hdl->wtodo == 0) {
		hdl->wstate = STATE_IDLE;
		hdl->wtodo = 0xdeadbeef;
	}
	return n;
}

static int
aucat_pollfd(struct sio_hdl *sh, struct pollfd *pfd, int events)
{
	struct aucat_hdl *hdl = (struct aucat_hdl *)sh;

	hdl->events = events;
	if (hdl->maxwrite <= 0)
		events &= ~POLLOUT;
	if (hdl->rstate == STATE_MSG)
		events |= POLLIN;
	pfd->fd = hdl->fd;
	pfd->events = events;		
	return 1;
}

static int
aucat_revents(struct sio_hdl *sh, struct pollfd *pfd)
{
	struct aucat_hdl *hdl = (struct aucat_hdl *)sh;
	int revents = pfd->revents;

	if (revents & POLLIN) {
		while (hdl->rstate == STATE_MSG) {
			if (!aucat_runmsg(hdl)) {
				revents &= ~POLLIN;
				break;
			}
		}
	}
	if (revents & POLLOUT) {
		if (hdl->maxwrite <= 0)
			revents &= ~POLLOUT;
	}
	if (hdl->sio.eof)
		return POLLHUP;
	return revents & (hdl->events | POLLHUP);
}

static int
aucat_setvol(struct sio_hdl *sh, unsigned vol)
{
	struct aucat_hdl *hdl = (struct aucat_hdl *)sh;

	hdl->reqvol = vol;
	return 1;
}

static void
aucat_getvol(struct sio_hdl *sh)
{
	struct aucat_hdl *hdl = (struct aucat_hdl *)sh;

	sio_onvol_cb(&hdl->sio, hdl->reqvol);
	return;
}
