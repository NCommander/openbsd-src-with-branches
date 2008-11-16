/*	$OpenBSD: listen.h,v 1.1 2008/10/26 08:49:44 ratchov Exp $	*/
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
#ifndef LISTEN_H
#define LISTEN_H

#include <sys/types.h>

#include "file.h"

struct listen {
	struct file file;
	char *path;
	int fd;
	int maxweight;		/* max dynamic range for clients */
};

struct listen *listen_new(struct fileops *, char *, int);
int listen_nfds(struct file *);
int listen_pollfd(struct file *, struct pollfd *, int events);
int listen_revents(struct file *, struct pollfd *);
void listen_close(struct file *);
extern struct fileops listen_ops;

#endif /* !defined(LISTEN_H) */
