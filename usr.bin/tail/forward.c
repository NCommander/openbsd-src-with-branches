/*	$OpenBSD: forward.c,v 1.14 2002/02/16 21:27:54 millert Exp $	*/
/*	$NetBSD: forward.c,v 1.7 1996/02/13 16:49:10 ghudson Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Sze-Tyan Wang.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)forward.c	8.1 (Berkeley) 6/6/93";
#endif
static char rcsid[] = "$OpenBSD: forward.c,v 1.14 2002/02/16 21:27:54 millert Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/event.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

static int rlines(FILE *, long, struct stat *);

/*
 * forward -- display the file, from an offset, forward.
 *
 * There are eight separate cases for this -- regular and non-regular
 * files, by bytes or lines and from the beginning or end of the file.
 *
 * FBYTES	byte offset from the beginning of the file
 *	REG	seek
 *	NOREG	read, counting bytes
 *
 * FLINES	line offset from the beginning of the file
 *	REG	read, counting lines
 *	NOREG	read, counting lines
 *
 * RBYTES	byte offset from the end of the file
 *	REG	seek
 *	NOREG	cyclically read characters into a wrap-around buffer
 *
 * RLINES
 *	REG	mmap the file and step back until reach the correct offset.
 *	NOREG	cyclically read lines into a wrap-around array of buffers
 */
void
forward(fp, style, off, sbp)
	FILE *fp;
	enum STYLE style;
	long off;
	struct stat *sbp;
{
	int ch;
	struct stat nsb;
	int kq;
	struct kevent ke;

	switch(style) {
	case FBYTES:
		if (off == 0)
			break;
		if (S_ISREG(sbp->st_mode)) {
			if (sbp->st_size < off)
				off = sbp->st_size;
			if (fseek(fp, off, SEEK_SET) == -1) {
				ierr();
				return;
			}
		} else while (off--)
			if ((ch = getc(fp)) == EOF) {
				if (ferror(fp)) {
					ierr();
					return;
				}
				break;
			}
		break;
	case FLINES:
		if (off == 0)
			break;
		for (;;) {
			if ((ch = getc(fp)) == EOF) {
				if (ferror(fp)) {
					ierr();
					return;
				}
				break;
			}
			if (ch == '\n' && !--off)
				break;
		}
		break;
	case RBYTES:
		if (S_ISREG(sbp->st_mode)) {
			if (sbp->st_size >= off &&
			    fseek(fp, -off, SEEK_END) == -1) {
				ierr();
				return;
			}
		} else if (off == 0) {
			while (getc(fp) != EOF)
				;
			if (ferror(fp)) {
				ierr();
				return;
			}
		} else {
			if (bytes(fp, off))
				return;
		}
		break;
	case RLINES:
		if (S_ISREG(sbp->st_mode)) {
			if (!off) {
				if (fseek(fp, 0L, SEEK_END) == -1) {
					ierr();
					return;
				}
			} else if (rlines(fp, off, sbp) != 0)
				lines(fp, off);
		} else if (off == 0) {
			while (getc(fp) != EOF)
				;
			if (ferror(fp)) {
				ierr();
				return;
			}
		} else {
			if (lines(fp, off))
				return;
		}
		break;
	}

	kq = -1;
kq_retry:
	if (fflag && ((kq = kqueue()) >= 0)) {
		ke.ident = fileno(fp);
		ke.flags = EV_ENABLE|EV_ADD|EV_CLEAR;
		ke.filter = EVFILT_READ;
		ke.fflags = ke.data = 0;
		ke.udata = NULL;
		if (kevent(kq, &ke, 1, NULL, 0, NULL) < 0) {
			close(kq);
			kq = -1;
		} else if (S_ISREG(sbp->st_mode)) {
			ke.ident = fileno(fp);
			ke.flags = EV_ENABLE|EV_ADD|EV_CLEAR;
			ke.filter = EVFILT_VNODE;
			ke.fflags = NOTE_DELETE | NOTE_RENAME;
			if (kevent(kq, &ke, 1, NULL, 0, NULL) < 0) {
				close(kq);
				kq = -1;
			}
		}
	}

	for (;;) {
		while (!feof(fp) && (ch = getc(fp)) != EOF)
			if (putchar(ch) == EOF)
				oerr();
		if (ferror(fp)) {
			ierr();
			if (kq != -1)
				close(kq);
			return;
		}
		(void)fflush(stdout);
		if (!fflag)
			break;
		clearerr(fp);
		if (kq < 0 || kevent(kq, NULL, 0, &ke, 1, NULL) <= 0) {
			sleep(1);
		} else if (ke.filter == EVFILT_READ) {
			continue;
		} else {
			/*
			 * File was renamed or deleted.
			 *
			 * Continue to look at it until a new file reappears
			 * with the same name. 
			 * Fall back to the old algorithm for that.
			 */
			close(kq);
			kq = -1;
		}

		if (is_stdin || stat(fname, &nsb) != 0)
			continue;
		/* Reopen file if the inode changes or file was truncated */
		if (nsb.st_ino != sbp->st_ino) {
			warnx("%s has been replaced, reopening.", fname);
			if ((fp = freopen(fname, "r", fp)) == NULL) {
				ierr();
				if (kq >= 0)
					close(kq);
				return;
			}
			(void)memcpy(sbp, &nsb, sizeof(nsb));
			goto kq_retry;
		} else if (nsb.st_size < sbp->st_size) {
			warnx("%s has been truncated, resetting.", fname);
			rewind(fp);
			(void)memcpy(sbp, &nsb, sizeof(nsb));
		}
	}
	if (kq >= 0)
		close(kq);
}

/*
 * rlines -- display the last offset lines of the file.
 */
static int
rlines(fp, off, sbp)
	FILE *fp;
	long off;
	struct stat *sbp;
{
	off_t size;
	char *p;
	char *start;

	if (!(size = sbp->st_size))
		return (0);

	if (size > SIZE_T_MAX)
		return (1);

	if ((start = mmap(NULL, (size_t)size, PROT_READ, MAP_PRIVATE,
	    fileno(fp), (off_t)0)) == MAP_FAILED)
		return (1);

	/* Last char is special, ignore whether newline or not. */
	for (p = start + size - 1; --size;)
		if (*--p == '\n' && !--off) {
			++p;
			break;
		}

	/* Set the file pointer to reflect the length displayed. */
	size = sbp->st_size - size;
	WR(p, size);
	if (fseek(fp, (long)sbp->st_size, SEEK_SET) == -1) {
		ierr();
		return (1);
	}
	if (munmap(start, (size_t)sbp->st_size)) {
		ierr();
		return (1);
	}

	return (0);
}
