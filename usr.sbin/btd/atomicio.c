/*	$OpenBSD: btd.c,v 1.1 2008/11/24 23:34:42 uwe Exp $	*/

/*
 * Copyright (c) 2003 Can Erkin Acar
 * Copyright (c) 2003 Anil Madhavapeddy <anil@recoil.org>
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

#include <errno.h>
#include <unistd.h>

#include "btd.h"

/* Read data with the assertion that it all must come through, or
 * else abort the process.  Based on atomicio() from openssh. */
int
atomic_read(int fd, void *buf, size_t n)
{
	char *s = buf;
	ssize_t res, pos = 0;

	while (n > pos) {
		res = read(fd, s + pos, n - pos);
		switch (res) {
		case -1:
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return -1;
		case 0:
			errno = 0;
			return -1;
		default:
			pos += res;
		}
	}

	return 0;
}

/* Write data with the assertion that it all has to be written, or
 * else abort the process.  Based on atomicio() from openssh. */
int
atomic_write(int fd, const void *buf, size_t n)
{
	const char *s = buf;
	ssize_t res, pos = 0;

	while (n > pos) {
		res = write(fd, s + pos, n - pos);
		switch (res) {
		case -1:
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return -1;
		case 0:
			errno = 0;
			return -1;
		default:
			pos += res;
		}
	}

	return 0;
}
