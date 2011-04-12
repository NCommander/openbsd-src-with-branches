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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "amsg.h"
#include "mio_priv.h"

struct mio_aucat_hdl {
	struct mio_hdl mio;
	int fd;
};

static void mio_aucat_close(struct mio_hdl *);
static size_t mio_aucat_read(struct mio_hdl *, void *, size_t);
static size_t mio_aucat_write(struct mio_hdl *, const void *, size_t);
static int mio_aucat_pollfd(struct mio_hdl *, struct pollfd *, int);
static int mio_aucat_revents(struct mio_hdl *, struct pollfd *);

static struct mio_ops mio_aucat_ops = {
	mio_aucat_close,
	mio_aucat_write,
	mio_aucat_read,
	mio_aucat_pollfd,
	mio_aucat_revents,
};

static struct mio_hdl *
mio_xxx_open(const char *str, char *sock, unsigned mode, int nbio)
{
	extern char *__progname;
	char unit[4], *sep, *opt;
	struct amsg msg;
	int s, n, todo;
	unsigned char *data;
	struct mio_aucat_hdl *hdl;
	struct sockaddr_un ca;
	socklen_t len = sizeof(struct sockaddr_un);
	uid_t uid;

	sep = strchr(str, '.');
	if (sep == NULL) {
		opt = "default";
		strlcpy(unit, str, sizeof(unit));
	} else {
		opt = sep + 1;
		if (sep - str >= sizeof(unit)) {
			DPRINTF("mio_aucat_open: %s: too long\n", str);
			return NULL;
		}
		strlcpy(unit, str, opt - str);
	}
	DPRINTF("mio_aucat_open: trying %s -> %s.%s\n", str, unit, opt);
	uid = geteuid();
	if (strchr(str, '/') != NULL)
		return NULL;
	snprintf(ca.sun_path, sizeof(ca.sun_path),
	    "/tmp/aucat-%u/%s%s", uid, sock, unit);
	ca.sun_family = AF_UNIX;

	hdl = malloc(sizeof(struct mio_aucat_hdl));
	if (hdl == NULL)
		return NULL;
	mio_create(&hdl->mio, &mio_aucat_ops, mode, nbio);

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0)
		goto bad_free;
	while (connect(s, (struct sockaddr *)&ca, len) < 0) {
		if (errno == EINTR)
			continue;
		DPERROR("mio_aucat_open: connect");
		/* try shared server */
		snprintf(ca.sun_path, sizeof(ca.sun_path),
		    "/tmp/aucat/%s%s", sock, unit);
		while (connect(s, (struct sockaddr *)&ca, len) < 0) {
			if (errno == EINTR)
				continue;
			DPERROR("mio_aucat_open: connect");
			goto bad_connect;
		}
		break;
 	}
	if (fcntl(s, F_SETFD, FD_CLOEXEC) < 0) {
		DPERROR("FD_CLOEXEC");
		goto bad_connect;
	}
	hdl->fd = s;

	/*
	 * say hello to server
	 */
	AMSG_INIT(&msg);
	msg.cmd = AMSG_HELLO;
	msg.u.hello.version = AMSG_VERSION;
	msg.u.hello.mode = mode;
	strlcpy(msg.u.hello.opt, opt, sizeof(msg.u.hello.opt));
	strlcpy(msg.u.hello.who, __progname, sizeof(msg.u.hello.who));
	n = write(s, &msg, sizeof(struct amsg));
	if (n < 0) {
		DPERROR("mio_aucat_open");
		goto bad_connect;
	}
	if (n != sizeof(struct amsg)) {
		DPRINTF("mio_aucat_open: short write\n");
		goto bad_connect;
	}
	todo = sizeof(struct amsg);
	data = (unsigned char *)&msg;
	while (todo > 0) {
		n = read(s, data, todo);
		if (n < 0) {
			DPERROR("mio_aucat_open");
			goto bad_connect;
		}
		if (n == 0) {
			DPRINTF("mio_aucat_open: eof\n");
			goto bad_connect;
		}
		todo -= n;
		data += n;
	}
	if (msg.cmd != AMSG_ACK) {
		DPRINTF("mio_aucat_open: proto error\n");
		goto bad_connect;
	}
	if (nbio && fcntl(hdl->fd, F_SETFL, O_NONBLOCK) < 0) {
		DPERROR("mio_aucat_open: fcntl(NONBLOCK)");
		goto bad_connect;
	}
	return (struct mio_hdl *)hdl;
 bad_connect:
	while (close(s) < 0 && errno == EINTR)
		; /* retry */
 bad_free:
	free(hdl);
	return NULL;
}

struct mio_hdl *
mio_midithru_open(const char *str, unsigned mode, int nbio)
{
	return mio_xxx_open(str, "midithru", mode, nbio);
}

struct mio_hdl *
mio_aucat_open(const char *str, unsigned mode, int nbio)
{
	return mio_xxx_open(str, "softaudio", mode, nbio);
}

static void
mio_aucat_close(struct mio_hdl *sh)
{
	struct mio_aucat_hdl *hdl = (struct mio_aucat_hdl *)sh;
	int rc;

	do {
		rc = close(hdl->fd);
	} while (rc < 0 && errno == EINTR);
	free(hdl);
}

static size_t
mio_aucat_read(struct mio_hdl *sh, void *buf, size_t len)
{
	struct mio_aucat_hdl *hdl = (struct mio_aucat_hdl *)sh;
	ssize_t n;

	while ((n = read(hdl->fd, buf, len)) < 0) {
		if (errno == EINTR)
			continue;
		if (errno != EAGAIN) {
			DPERROR("mio_aucat_read: read");
			hdl->mio.eof = 1;
		}
		return 0;
	}
	if (n == 0) {
		DPRINTF("mio_aucat_read: eof\n");
		hdl->mio.eof = 1;
		return 0;
	}
	return n;
}

static size_t
mio_aucat_write(struct mio_hdl *sh, const void *buf, size_t len)
{
	struct mio_aucat_hdl *hdl = (struct mio_aucat_hdl *)sh;
	ssize_t n;

	while ((n = write(hdl->fd, buf, len)) < 0) {
		if (errno == EINTR)
			continue;
		if (errno != EAGAIN) {
			DPERROR("mio_aucat_write: write");
			hdl->mio.eof = 1;
		}
 		return 0;
	}
	return n;
}

static int
mio_aucat_pollfd(struct mio_hdl *sh, struct pollfd *pfd, int events)
{
	struct mio_aucat_hdl *hdl = (struct mio_aucat_hdl *)sh;

	pfd->fd = hdl->fd;
	pfd->events = events;
	return 1;
}

static int
mio_aucat_revents(struct mio_hdl *sh, struct pollfd *pfd)
{
	return pfd->revents;
}
