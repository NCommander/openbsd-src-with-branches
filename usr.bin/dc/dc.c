/*	$OpenBSD: dc.c,v 1.3 2003/09/19 19:06:29 deraadt Exp $	*/

/*
 * Copyright (c) 2003, Otto Moerbeek <otto@drijf.net>
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

#ifndef lint
static const char rcsid[] = "$OpenBSD: dc.c,v 1.3 2003/09/19 19:06:29 deraadt Exp $";
#endif /* not lint */

#include <err.h>
#include <unistd.h>

#include "extern.h"

static __dead void	usage(void);

extern char		*__progname;

static __dead void
usage(void)
{
	fprintf(stderr, "usage: %s [-x] [file]\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int		ch;
	bool		extended_regs = false;
	FILE		*file;
	struct source	src;

	/* accept and ignore a single dash to be 4.4BSD dc(1) compatible */
	while ((ch = getopt(argc, argv, "x-")) != -1) {
		switch (ch) {
		case 'x':
			extended_regs = true;
			break;
		case '-':
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	init_bmachine(extended_regs);
	setlinebuf(stdout);
	setlinebuf(stderr);

	if (argc > 1)
		usage();
	else if (argc == 1) {
		file = fopen(argv[0], "r");
		if (file == NULL)
			err(1, "cannot open file %s", argv[0]);
		src_setstream(&src, file);
		reset_bmachine(&src);
		eval();
		fclose(file);
	}
	/*
	 * BSD dc and Solaris dc continue with stdin after processing
	 * the file given as the argument.
	 */
	src_setstream(&src, stdin);
	reset_bmachine(&src);
	eval();

	return 0;
}
