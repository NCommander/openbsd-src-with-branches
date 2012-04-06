/*	$OpenBSD: intercept-translate.c,v 1.15 2012/03/02 17:23:08 matthew Exp $	*/
/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/tree.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <err.h>

#include "intercept.h"

char *error_msg = "error";

static void ic_trans_free(struct intercept_translate *);
static int ic_print_filename(char *, size_t, struct intercept_translate *);
static int ic_get_filename(struct intercept_translate *, int, pid_t, void *);
static int ic_get_string(struct intercept_translate *, int, pid_t, void *);
static int ic_get_linkname(struct intercept_translate *, int, pid_t, void *);
static int ic_get_sockaddr(struct intercept_translate *, int, pid_t, void *);
static int ic_print_sockaddr(char *, size_t, struct intercept_translate *);

static void
ic_trans_free(struct intercept_translate *trans)
{
	if (trans->trans_data)
		free(trans->trans_data);
	if (trans->trans_print)
		free(trans->trans_print);
	trans->trans_valid = 0;
	trans->trans_data = NULL;
	trans->trans_print = NULL;
	trans->trans_size = 0;
	trans->trans_addr = NULL;
}

extern struct intercept_system intercept;

/* Takes a translation structure and retrieves the right data */

int
intercept_translate(struct intercept_translate *trans,
    int fd, pid_t pid, int off, void *args, int argsize)
{
	void *addr, *addr2, *addrend;

	ic_trans_free(trans);

	if (intercept.getarg(off, args, argsize, &addr) == -1)
		return (-1);
	if (trans->off2) {
		if (intercept.getarg(trans->off + trans->off2,
			args, argsize, &addr2) == -1)
			return (-1);
		trans->trans_addr2 = addr2;
	}
	if (trans->offend) {
		/* XXX: Abstraction violation. */
		int numargs = argsize / sizeof(register_t);

		if (intercept.getarg(numargs + trans->offend,
		    args, argsize, &addrend) == -1)
			return (-1);
		trans->trans_addrend = addrend;
	}

	trans->trans_valid = 1;
	trans->trans_addr = addr;

	if (trans->translate == NULL)
		return (0);

	if ((*trans->translate)(trans, fd, pid, addr) == -1) {
		trans->trans_valid = 0;
		return (-1);
	}

	return (0);
}

char *
intercept_translate_print(struct intercept_translate *trans)
{
	char line[_POSIX2_LINE_MAX];

	if (trans->trans_print == NULL) {
		if (trans->print(line, sizeof(line), trans) == -1)
			return (error_msg);

		if ((trans->trans_print = strdup(line)) == NULL)
			return (error_msg);
	}

	return (trans->trans_print);
}

static int
ic_print_filename(char *buf, size_t buflen, struct intercept_translate *tl)
{
	strlcpy(buf, tl->trans_data, buflen);

	return (0);
}

static int
ic_get_filename(struct intercept_translate *trans, int fd, pid_t pid,
    void *addr)
{
	char *name;
	size_t len;

	name = intercept_filename(fd, pid, addr, ICLINK_ALL, NULL);
	if (name == NULL)
		return (-1);

	len = strlen(name) + 1;
	trans->trans_data = malloc(len);
	if (trans->trans_data == NULL)
		return (-1);

	trans->trans_size = len;
	memcpy(trans->trans_data, name, len);
	trans->trans_flags = ICTRANS_NOLINKS;

	return (0);
}

static int
ic_get_string(struct intercept_translate *trans, int fd, pid_t pid, void *addr)
{
	char *name;
	size_t len;

	if (addr == NULL)
		return (-1);

	name = intercept_get_string(fd, pid, addr);
	if (name == NULL)
		return (-1);

	len = strlen(name) + 1;
	trans->trans_data = malloc(len);
	if (trans->trans_data == NULL)
		return (-1);

	trans->trans_size = len;
	memcpy(trans->trans_data, name, len);

	return (0);
}

static int
ic_get_linkname(struct intercept_translate *trans, int fd, pid_t pid,
    void *addr)
{
	char *name;
	size_t len;

	name = intercept_filename(fd, pid, addr, ICLINK_NONE, NULL);
	if (name == NULL)
		return (-1);

	len = strlen(name) + 1;
	trans->trans_data = malloc(len);
	if (trans->trans_data == NULL)
		return (-1);

	trans->trans_size = len;
	memcpy(trans->trans_data, name, len);

	return (0);
}

/* Resolves all symlinks but for the last component */

static int
ic_get_unlinkname(struct intercept_translate *trans, int fd, pid_t pid,
    void *addr)
{
	char *name;
	size_t len;

	name = intercept_filename(fd, pid, addr, ICLINK_NOLAST, NULL);
	if (name == NULL)
		return (-1);

	len = strlen(name) + 1;
	trans->trans_data = malloc(len);
	if (trans->trans_data == NULL)
		return (-1);

	trans->trans_size = len;
	memcpy(trans->trans_data, name, len);
	trans->trans_flags = ICTRANS_NOLINKS;

	return (0);
}

static int
ic_get_filenameat(struct intercept_translate *trans, int fd, pid_t pid,
    void *addr)
{
	char *name;
	size_t len;
	int atfd = (intptr_t)trans->trans_addr2;
	int follow = (intptr_t)trans->user;
	int userp;

	if (trans->offend) {
		int flag = (intptr_t)trans->trans_addrend;
		if ((flag & ~(AT_SYMLINK_FOLLOW | AT_SYMLINK_NOFOLLOW)) != 0)
			return (-1);
		if ((flag & follow) != 0)
			return (-1);
		if (flag != 0)
			follow = flag;
	}

	userp = (follow == AT_SYMLINK_FOLLOW) ? ICLINK_ALL : ICLINK_NOLAST;
	name = intercept_filenameat(fd, pid, atfd, addr, userp, NULL);
	if (name == NULL)
		return (-1);

	len = strlen(name) + 1;
	trans->trans_data = malloc(len);
	if (trans->trans_data == NULL)
		return (-1);

	trans->trans_size = len;
	memcpy(trans->trans_data, name, len);
	trans->trans_flags = ICTRANS_NOLINKS;

	return (0);
}

static int
ic_get_sockaddr(struct intercept_translate *trans, int fd, pid_t pid,
    void *addr)
{
	struct sockaddr_storage sa;
	socklen_t len;

	len = (intptr_t)trans->trans_addr2;
	if (len == 0 || len > sizeof(struct sockaddr_storage))
		return (-1);

	if (intercept.io(fd, pid, INTERCEPT_READ, addr,
		(void *)&sa, len) == -1)
		return (-1);

	trans->trans_data = malloc(len);
	if (trans->trans_data == NULL)
		return (-1);
	trans->trans_size = len;
	memcpy(trans->trans_data, &sa, len);

	return (0);
}

#ifndef offsetof
#define offsetof(s, e)	((size_t)&((s *)0)->e)
#endif

static int
ic_print_sockaddr(char *buf, size_t buflen, struct intercept_translate *tl)
{
	char host[NI_MAXHOST];
	char serv[NI_MAXSERV];
	struct sockaddr *sa = tl->trans_data;
	socklen_t len = (socklen_t)tl->trans_size;

	buf[0] = '\0';

	switch (sa->sa_family) {
	case PF_LOCAL:
		if (len <= offsetof(struct sockaddr, sa_data))
			return (-1);
		len -= offsetof(struct sockaddr, sa_data);
		if (buflen < len + 1)
			len = buflen - 1;
		memcpy(buf, sa->sa_data, len);
		buf[len] = '\0';
		return (0);
	case PF_INET:
	case PF_INET6:
		break;
	default:
		snprintf(buf, buflen, "family(%d)", sa->sa_family);
		return (0);
	}

	sa->sa_len = len;
	if (getnameinfo(sa, len,
		host, sizeof(host), serv, sizeof(serv),
		NI_NUMERICHOST | NI_NUMERICSERV)) {
		warn("getnameinfo");
		return (-1);
	}

	snprintf(buf, buflen, "inet-[%s]:%s", host, serv);

	return (0);
}

static int
ic_get_msghdr(struct intercept_translate *trans, int fd, pid_t pid,
    void *addr)
{
	struct msghdr msg;
	int len = sizeof(struct msghdr);

	if (intercept.io(fd, pid, INTERCEPT_READ, addr,
	    (void *)&msg, len) == -1)
		return (-1);

	len = msg.msg_namelen;
	if (msg.msg_name == NULL || len <= 0 || len > 2048) {
		trans->trans_data = NULL;
		trans->trans_size = 0;
		return (0);
	}

	trans->trans_size = len;
	trans->trans_data = malloc(len);
	if (trans->trans_data == NULL)
		return (-1);
	if (intercept.io(fd, pid, INTERCEPT_READ, msg.msg_name,
	    (void *)trans->trans_data, trans->trans_size) == -1)
		return (-1);

	return (0);
}

static int
ic_print_msghdr(char *buf, size_t buflen, struct intercept_translate *tl)
{
	int res = 0;

	if (tl->trans_size == 0) {
		snprintf(buf, buflen, "<unknown>");
	} else {
		res = ic_print_sockaddr(buf, buflen, tl);
		/*
		 * disable replacement of this argument because it's two levels
		 * deep and we cant replace that fast.
		 */
		tl->trans_size = 0;

		/* TODO: make this less of a hack */
	}

	return (res);
}

struct intercept_translate ic_translate_string = {
	"string",
	ic_get_string, ic_print_filename,
};

struct intercept_translate ic_translate_filename = {
	"filename",
	ic_get_filename, ic_print_filename,
};

struct intercept_translate ic_translate_linkname = {
	"filename",
	ic_get_linkname, ic_print_filename,
};

struct intercept_translate ic_translate_unlinkname = {
	"filename",
	ic_get_unlinkname, ic_print_filename,
};

struct intercept_translate ic_translate_filenameat = {
	"filename",
	ic_get_filenameat, ic_print_filename,
	.off2 = -1,
	.user = (void *)AT_SYMLINK_FOLLOW,
};

struct intercept_translate ic_translate_unlinknameat = {
	"filename",
	ic_get_filenameat, ic_print_filename,
	.off2 = -1,
	.user = (void *)AT_SYMLINK_NOFOLLOW,
};

struct intercept_translate ic_translate_filenameatflag = {
	"filename",
	ic_get_filenameat, ic_print_filename,
	.off2 = -1,
	.offend = -1,
	.user = (void *)AT_SYMLINK_FOLLOW,
};

struct intercept_translate ic_translate_unlinknameatflag = {
	"filename",
	ic_get_filenameat, ic_print_filename,
	.off2 = -1,
	.offend = -1,
	.user = (void *)AT_SYMLINK_NOFOLLOW,
};

struct intercept_translate ic_translate_connect = {
	"sockaddr",
	ic_get_sockaddr, ic_print_sockaddr,
	/* XXX - Special handling */ 1,
};

struct intercept_translate ic_translate_sendmsg = {
	"sockaddr",
	ic_get_msghdr, ic_print_msghdr,
};
