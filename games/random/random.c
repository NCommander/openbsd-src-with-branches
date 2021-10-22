/*	$OpenBSD: random.c,v 1.19 2016/01/10 13:35:10 mestre Exp $	*/
/*	$NetBSD: random.c,v 1.3 1995/04/22 07:44:05 cgd Exp $	*/

/*
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Guy Harris at Network Appliance Corp.
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

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

__dead void usage(void);

int
main(int argc, char *argv[])
{
	double denom;
	int ch, random_exit, selected, unbuffer_output;
	char *ep;

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	random_exit = unbuffer_output = 0;
	while ((ch = getopt(argc, argv, "erh")) != -1)
		switch (ch) {
		case 'e':
			random_exit = 1;
			break;
		case 'r':
			unbuffer_output = 1;
			break;
		default:
		case 'h':
			usage();
		}

	argc -= optind;
	argv += optind;

	switch (argc) {
	case 0:
		denom = 2;
		break;
	case 1:
		errno = 0;
		denom = strtod(*argv, &ep);
		if (errno == ERANGE)
			err(1, "%s", *argv);
		if (denom == 0 || *ep != '\0')
			errx(1, "denominator is not valid.");
		break;
	default:
		usage(); 
	}

	/* Compute a random exit status between 0 and denom - 1. */
	if (random_exit)
		return (arc4random_uniform(denom));

	/*
	 * Act as a filter, randomly choosing lines of the standard input
	 * to write to the standard output.
	 */
	if (unbuffer_output)
		setvbuf(stdout, NULL, _IONBF, 0);
	
	/*
	 * Select whether to print the first line.  (Prime the pump.)
	 * We find a random number between 0 and denom - 1 and, if it's
	 * 0 (which has a 1 / denom chance of being true), we select the
	 * line.
	 */
	selected = arc4random_uniform(denom) == 0;
	while ((ch = getchar()) != EOF) {
		if (selected)
			(void)putchar(ch);
		if (ch == '\n') {
			/* End of that line.  See if we got an error. */
			if (ferror(stdout))
				err(2, "stdout");

			/* Now see if the next line is to be printed. */
			selected = arc4random_uniform(denom) == 0;
		}
	}
	if (ferror(stdin))
		err(2, "stdin");
	return 0;
}

void
usage(void)
{

	(void)fprintf(stderr, "usage: %s [-er] [denominator]\n", getprogname());
	exit(1);
}
