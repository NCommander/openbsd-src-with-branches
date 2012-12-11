/*
 * Copyright (c) 2012 Damien Miller <djm@mindrot.org>
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
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <err.h>
#include <errno.h>

static void
usage(void)
{
	fprintf(stderr, "Usage: modpipe [-m modspec ...] < in > out\n");
	fprintf(stderr, "modspec is one of:\n");
	fprintf(stderr, "    xor:offset:value       - XOR \"value\" at \"offset\"\n");
	fprintf(stderr, "    andor:offset:val1:val2 - AND \"val1\" then OR \"val2\" at \"offset\"\n");
	exit(1);
}

#define MAX_MODIFICATIONS 256
struct modification {
	enum { MOD_XOR, MOD_AND_OR } what;
	u_int64_t offset;
	u_int8_t m1, m2;
};

static void
parse_modification(const char *s, struct modification *m)
{
	char what[16+1];
	int n;

	bzero(m, sizeof(*m));
	if ((n = sscanf(s, "%16[^:]%*[:]%lli%*[:]%hhi%*[:]%hhi",
	    what, &m->offset, &m->m1, &m->m2)) < 3)
		errx(1, "Invalid modification spec \"%s\"", s);
	if (strcasecmp(what, "xor") == 0) {
		m->what = MOD_XOR;
		if (n > 3)
			errx(1, "Invalid modification spec \"%s\"", s);
	} else if (strcasecmp(what, "andor") == 0) {
		m->what = MOD_AND_OR;
		if (n != 4)
			errx(1, "Invalid modification spec \"%s\"", s);
	} else
		errx(1, "Invalid modification type \"%s\"", what);
}

int
main(int argc, char **argv)
{
	int ch;
	u_char buf[8192];
	size_t total;
	ssize_t r, s, o;
	struct modification mods[MAX_MODIFICATIONS];
	u_int i, num_mods = 0;

	while ((ch = getopt(argc, argv, "m:")) != -1) {
		switch (ch) {
		case 'm':
			if (num_mods >= MAX_MODIFICATIONS)
				errx(1, "Too many modifications");
			parse_modification(optarg, &(mods[num_mods++]));
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	for (total = 0;;) {
		r = s = read(STDIN_FILENO, buf, sizeof(buf));
		if (r == 0)
			return 0;
		if (r < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			err(1, "read");
		}
		for (i = 0; i < num_mods; i++) {
			if (mods[i].offset < total ||
			    mods[i].offset >= total + s)
				continue;
			switch (mods[i].what) {
			case MOD_XOR:
				buf[mods[i].offset - total] ^= mods[i].m1;
				break;
			case MOD_AND_OR:
				buf[mods[i].offset - total] &= mods[i].m1;
				buf[mods[i].offset - total] |= mods[i].m2;
				break;
			}
		}
		for (o = 0; o < s; o += r) {
			r = write(STDOUT_FILENO, buf, s - o);
			if (r == 0)
				return 0;
			if (r < 0) {
				if (errno == EAGAIN || errno == EINTR)
					continue;
				err(1, "write");
			}
		}
		total += s;
	}
	return 0;
}
