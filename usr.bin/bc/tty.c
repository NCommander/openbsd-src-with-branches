/*      $OpenBSD: scan.l,v 1.27 2011/08/03 08:48:19 otto Exp $	*/

/*
 * Copyright (c) 2013, Otto Moerbeek <otto@drijf.net>
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
#include <signal.h>
#include <histedit.h>
#include <termios.h>
#include "extern.h"

struct termios ttysaved, ttyedit;

int
settty(struct termios *t)
{
	int ret;

	while ((ret = tcsetattr(0, TCSADRAIN,  t) == -1) && errno == EINTR)
		continue;
	return ret;
}

int
gettty(struct termios *t)
{
	int ret;

	while ((ret = tcgetattr(0, t) == -1) && errno == EINTR)
		continue;
	return ret;
}

/* ARGSUSED */
void
tstpcont(int sig)
{
	int save_errno = errno;

	if (sig == SIGTSTP) {
		signal(SIGCONT, tstpcont);
		gettty(&ttyedit);
		settty(&ttysaved);		
	} else {
		signal(SIGTSTP, tstpcont);
		settty(&ttyedit);		
	}
	signal(sig, SIG_DFL);
	kill(0, sig);
	errno = save_errno;
}
