/*	$OpenBSD$	*/
/*
 * Copyright (c) 2022 Job Snijders <job@openbsd.org>
 * Copyright (c) 2022 Claudio Jeker <claudio@openbsd.org>
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

#include <sys/cdefs.h>
#include <sys/time.h>

#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

static char		*format = "%b %d %H:%M:%S";
static char		*buf;
static char		*outbuf;
static size_t		 bufsize;

static void		 fmtfmt(struct tm *, long);
static void __dead	 usage(void);

int
main(int argc, char *argv[])
{
	int iflag, sflag;
	int ch, prev;
	struct timespec start, now, elapsed;
	struct tm *lt, tm;

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	iflag = sflag = 0;

	while ((ch = getopt(argc, argv, "is")) != -1) {
		switch (ch) {
		case 'i':
			iflag = 1;
			format = "%H:%M:%S";
			break;
		case 's':
			sflag = 1;
			format = "%H:%M:%S";
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if ((iflag && sflag) || argc > 1)
		usage();

	if (argc == 1)
		format = *argv;

	bufsize = strlen(format);
	if (bufsize > SIZE_MAX / 10)
		errx(1, "format string too big");

	bufsize *= 10;
	if ((buf = calloc(1, bufsize)) == NULL)
		err(1, NULL);
	if ((outbuf = calloc(1, bufsize)) == NULL)
		err(1, NULL);

	clock_gettime(CLOCK_MONOTONIC, &start);

	for (prev = '\n'; (ch = getchar()) != EOF; prev = ch) {
		if (prev == '\n') {
			if (iflag || sflag) {
				if (clock_gettime(CLOCK_MONOTONIC, &now))
					err(1, "clock_gettime");
				timespecsub(&now, &start, &elapsed);
				if (gmtime_r(&elapsed.tv_sec, &tm) == NULL)
					err(1, "gmtime_r");
				if (iflag)
					clock_gettime(CLOCK_MONOTONIC, &start);
				fmtfmt(&tm, elapsed.tv_nsec);
			} else {
				if (clock_gettime(CLOCK_REALTIME, &now))
					err(1, "clock_gettime");
				lt = localtime(&now.tv_sec);
				if (lt == NULL)
					err(1, "localtime");
				fmtfmt(lt, now.tv_nsec);
			}
		}
		if (putchar(ch) == EOF)
			break;
	}

	if (fclose(stdout))
		err(1, "stdout");
	return 0;
}

static void __dead
usage(void)
{
	fprintf(stderr, "usage: %s [-i | -s] [format]\n", getprogname());
	exit(1);
}

/*
 * yo dawg, i heard you like format strings
 * so i put format strings in your user supplied input
 * so you can format while you format
 */
static void
fmtfmt(struct tm *tm, long tv_nsec)
{
	char *f, ms[7];

	snprintf(ms, sizeof(ms), "%06ld", tv_nsec / 1000);
	strlcpy(buf, format, bufsize);
	f = buf;

	do {
		while ((f = strchr(f, '%')) != NULL && f[1] == '%')
			f += 2;

		if (f == NULL)
			break;

		f++;
		if (f[0] == '.' &&
		    (f[1] == 'S' || f[1] == 's' || f[1] == 'T')) {
			size_t l;

			f[0] = f[1];
			f[1] = '.';
			f += 2;
			l = strlen(f);
			memmove(f + 6, f, l + 1);
			memcpy(f, ms, 6);
			f += 6;
		}
	} while (*f != '\0');

	if (strftime(outbuf, bufsize, buf, tm) == 0)
		errx(1, "strftime");

	fprintf(stdout, "%s ", outbuf);
	if (ferror(stdout))
		exit(1);
}
