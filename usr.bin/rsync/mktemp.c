/*	$OpenBSD: mktemp.c,v 1.39 2017/11/28 06:55:49 tb Exp $ */
/*
 * Copyright (c) 1996-1998, 2008 Theo de Raadt
 * Copyright (c) 1997, 2008-2009 Todd C. Miller
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
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#define MKTEMP_NAME	0
#define MKTEMP_FILE	1
#define MKTEMP_DIR	2
#define MKTEMP_LINK	3

#define TEMPCHARS	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
#define NUM_CHARS	(sizeof(TEMPCHARS) - 1)
#define MIN_X		6

#define MKOTEMP_FLAGS	(O_APPEND | O_CLOEXEC | O_DSYNC | O_RSYNC | O_SYNC)

#ifndef nitems
#define nitems(_a)	(sizeof((_a)) / sizeof((_a)[0]))
#endif

/* adapted from libc/stdio/mktemp.c */
static int
mktemp_internalat(int pfd, char *path, int slen, int mode, int flags,
    char *link)
{
	char *start, *cp, *ep;
	const char tempchars[] = TEMPCHARS;
	unsigned int tries;
	struct stat sb;
	size_t len;
	int fd;

	len = strlen(path);
	if (len < MIN_X || slen < 0 || (size_t)slen > len - MIN_X) {
		errno = EINVAL;
		return(-1);
	}
	ep = path + len - slen;

	for (start = ep; start > path && start[-1] == 'X'; start--)
		;
	if (ep - start < MIN_X) {
		errno = EINVAL;
		return(-1);
	}

	if (flags & ~MKOTEMP_FLAGS) {
		errno = EINVAL;
		return(-1);
	}
	flags |= O_CREAT | O_EXCL | O_RDWR;

	tries = INT_MAX;
	do {
		cp = start;
		do {
			unsigned short rbuf[16];
			unsigned int i;

			/*
			 * Avoid lots of arc4random() calls by using
			 * a buffer sized for up to 16 Xs at a time.
			 */
			arc4random_buf(rbuf, sizeof(rbuf));
			for (i = 0; i < nitems(rbuf) && cp != ep; i++)
				*cp++ = tempchars[rbuf[i] % NUM_CHARS];
		} while (cp != ep);

		switch (mode) {
		case MKTEMP_NAME:
			if (fstatat(pfd, path, &sb, AT_SYMLINK_NOFOLLOW) != 0)
				return(errno == ENOENT ? 0 : -1);
			break;
		case MKTEMP_FILE:
			fd = openat(pfd, path, flags, S_IRUSR|S_IWUSR);
			if (fd != -1 || errno != EEXIST)
				return(fd);
			break;
		case MKTEMP_DIR:
			if (mkdirat(pfd, path, S_IRUSR|S_IWUSR|S_IXUSR) == 0)
				return(0);
			if (errno != EEXIST)
				return(-1);
			break;
		case MKTEMP_LINK:
			if (symlinkat(link, pfd, path) == 0)
				return(0);
			else if (errno != EEXIST)
				return(-1);
			break;
		}
	} while (--tries);

	errno = EEXIST;
	return(-1);
}

/*
 * A combination of mkstemp(3) and openat(2).
 * On success returns a file descriptor and trailing Xs are overwritten in
 * path to create a unique file name.
 * Returns -1 on failure.
 */
int
mkstempat(int fd, char *path)
{
	return(mktemp_internalat(fd, path, 0, MKTEMP_FILE, 0, NULL));
}

/*
 * A combination of mkstemp(3) and symlinkat(2).
 * On success returns path with trailing Xs overwritten to create a unique
 * file name.
 * Returns NULL on failure.
 */
char*
mkstemplinkat(char *link, int fd, char *path)
{
	if (mktemp_internalat(fd, path, 0, MKTEMP_LINK, 0, link) == -1)
		return(NULL);
	return(path);
}

/*
 * Turn path into a suitable template for mkstemp*at functions and
 * place it into the newly allocated string returned in ret.
 * The caller must free ret.
 * Returns -1 on failure or number of characters output to ret
 * (excluding the final '\0').
 */
int
mktemplate(char **ret, const char *path, int recursive)
{
	int		 n, dirlen;
	const char	*cp;

	if (recursive && (cp = strrchr(path, '/')) != NULL) {
		dirlen = cp - path;
		if ((n = asprintf(ret, "%.*s/.%s.XXXXXXXXXX", dirlen, path,
		    path + dirlen + 1)) == -1)
			*ret = NULL;
	} else {
		if ((n = asprintf(ret, ".%s.XXXXXXXXXX", path)) == -1)
			*ret = NULL;
	}
	return(n);
}
